#include "spritesheet.hpp"

#include <SFML/Graphics.hpp>
#include <random>
#include <functional>

// dynamic sized matrix
template<typename T>
struct Matrix
{
	Matrix() = default;
	Matrix(const sf::Vector2u & _size) : size(_size)
	{
		elements.resize(size.x * size.y);
	}

	T& operator()(unsigned x, unsigned y) { return elements[flatIndex(x,y)]; }
	size_t flatIndex(unsigned x, unsigned y) const { return x + y * size.x; }
	sf::Vector2u index(size_t flat) const 
	{ 
		return sf::Vector2u(static_cast<unsigned>(flat % size.x), static_cast<unsigned>(flat / size.y)); 
	}

	sf::Vector2u size;
	std::vector<T> elements;
};

using TransferMap = Matrix<sf::Vector2u>;
sf::Image applyMap(const TransferMap& _map, const sf::Image& _src)
{
	sf::Image image;
	image.create(_src.getSize().x, _src.getSize().y, sf::Color::Black);
	for (unsigned y = 0; y < image.getSize().y; ++y)
	{
		const unsigned idy = y * image.getSize().x;
		for (unsigned x = 0; x < image.getSize().x; ++x)
		{
			const sf::Vector2u& srcPos = _map.elements[x + idy];
			image.setPixel(x, y, _src.getPixel(srcPos.x, srcPos.y));
		}
	}
	return image;
}

enum struct PaddingMode
{
	Zero,
};

struct Convolution
{
	sf::Vector2u kernelSize;
	PaddingMode padding;
};

sf::Color getPixelPadded(const sf::Image& _image, int x, int y)
{
	if (x < 0 || y < 0
		|| x >= static_cast<int>(_image.getSize().x)
		|| y >= static_cast<int>(_image.getSize().y))
	{
		return sf::Color(0u);
	}

	return _image.getPixel(x, y);
}


template<typename T, typename Distance, typename Reduce>
auto applyConvolution(const sf::Image& _image, const Matrix<T>& _kernel, Distance _dist, Reduce _reduce)
{
	using ReturnType = decltype(std::function{ _reduce })::result_type;

	sf::Vector2i kernelHalf(_kernel.size.x / 2, _kernel.size.y / 2);
	auto computeKernel = [&](unsigned x, unsigned y)
	{
		ReturnType sum{};
		for (unsigned i = 0; i < _kernel.size.y; ++i)
			for (unsigned j = 0; j < _kernel.size.x; ++j)
			{
				// use padding if index is negative
				const int xInd = static_cast<int>(x + j) - kernelHalf.x;
				const int yInd = static_cast<int>(y + i) - kernelHalf.y;
				const auto d = _dist(_kernel.elements[j + i * _kernel.size.x], getPixelPadded(_image, xInd, yInd));
				sum = _reduce(sum, d);
			}
		return sum;
	};

	Matrix<ReturnType> result(_image.getSize());

	const sf::Vector2u size = _image.getSize();
	for (unsigned y = 0; y < size.y; ++y)
		for (unsigned x = 0; x < size.x; ++x)
		{
			result(x,y) = computeKernel(x, y);
		}

	return result;
}

// constructs a map that transforms _src to _dst.
TransferMap constructMap(const sf::Image& _src, const sf::Image& _dst)
{
	const sf::Vector2u size = _src.getSize();

	const sf::Vector2u kernelSize(3, 3);
	sf::Vector2i kernelHalf(kernelSize.x / 2, kernelSize.y / 2);
	auto makeKernel = [&](unsigned x, unsigned y)
	{
		Matrix<sf::Color> kernel(kernelSize);

		for (unsigned i = 0; i < kernelSize.y; ++i)
			for (unsigned j = 0; j < kernelSize.x; ++j)
			{
				const int xInd = static_cast<int>(x + j) - kernelHalf.x;
				const int yInd = static_cast<int>(y + i) - kernelHalf.y;
				kernel(j,i) = getPixelPadded(_src, xInd, yInd);
			}

		return kernel;
	};

	auto distance = [](const sf::Color& c1, const sf::Color& c2)
	{
		return c1 == c2 ? 1 : 0;
	};

	auto sum = [](int a, int b)
	{
		return a + b;
	};

	TransferMap map(size);
	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			Matrix<int> similarity = applyConvolution(_dst, makeKernel(x,y), distance, sum);

			// default guess is identity
			sf::Vector2u maxPos(x,y);
			int max = similarity(x,y);
			for (size_t i = 0; i < similarity.elements.size(); ++i)
			{
				if (max < similarity.elements[i])
				{
					max = similarity.elements[i];
					maxPos = similarity.index(i);
				}
			}
			map(x, y) = maxPos;
		}
	}

	return map;
}

sf::Image imageDifference(const sf::Image& a, const sf::Image& b)
{
	sf::Image difImage;
	const sf::Vector2u size = a.getSize();
	difImage.create(size.x, size.y);

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			sf::Color dif;
			const sf::Color col1 = a.getPixel(x, y);
			const sf::Color col2 = b.getPixel(x, y);

			dif.r = std::abs(static_cast<int>(col1.r) - static_cast<int>(col2.r));
			dif.g = std::abs(static_cast<int>(col1.g) - static_cast<int>(col2.g));
			dif.b = std::abs(static_cast<int>(col1.b) - static_cast<int>(col2.b));
			dif.a = 255;
			difImage.setPixel(x,y, dif);
		}
	}
	return difImage;
}

int main()
{
	SpriteSheet reference("../Sprites/HumanMale_ApronChest.png", 8);
	SpriteSheet validation("../Sprites/HumanMale_FamilyChest_Medium.png", 8);
	validation.save("yolo.png");

	SpriteSheet output;
	SpriteSheet dif;
	output.frames.emplace_back(validation.frames[0]);
	dif.frames.emplace_back(imageDifference(validation.frames[0], output.frames[0]));
	for (int i = 1; i < 8; ++i)
	{
		const TransferMap map = constructMap(reference.frames[0], reference.frames[i]);
		output.frames.emplace_back(applyMap(map, validation.frames[i]));
		dif.frames.emplace_back(imageDifference(validation.frames[i], output.frames.back()));
	}
/*	TransferMap transferMap(frameSize);
	for (unsigned y = 0; y < frameSize.y; ++y)
	{
		const unsigned idy = y * frameSize.x;
		for (unsigned x = 0; x < frameSize.x; ++x)
		{
			transferMap.elements[x + idy] = sf::Vector2u(x, frameSize.y - y - 1);
		}
	}*/
	dif.save("dif.png");
	output.save("output.png");

	return 0;
}