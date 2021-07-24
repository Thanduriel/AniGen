#include "map.hpp"
#include "math/convolution.hpp"
#include "math/vectorext.hpp"
#include "utils.hpp"
#include "spritesheet.hpp"
#include "colors.hpp"

#include <numeric>
#include <iostream>

using namespace math;

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

// constructs a map that transforms _src to _dst.
TransferMap constructMap(const sf::Image& _src, const sf::Image& _dst, unsigned _numThreads)
{
	const sf::Vector2u size = _src.getSize();

	const sf::Vector2u kernelSize(3, 3);
	const sf::Vector2i kernelHalf(kernelSize.x / 2, kernelSize.y / 2);
	auto makeKernel = [&](unsigned x, unsigned y)
	{
		Matrix<sf::Color> kernel(kernelSize);

		for (unsigned i = 0; i < kernelSize.y; ++i)
			for (unsigned j = 0; j < kernelSize.x; ++j)
			{
				const int xInd = static_cast<int>(x + j) - kernelHalf.x;
				const int yInd = static_cast<int>(y + i) - kernelHalf.y;
				kernel(j, i) = getPixelPadded(_src, xInd, yInd);
			}

		return kernel;
	};

	auto distance = [](const sf::Color& c1, const sf::Color& c2)
	{
		return c1 == c2 ? 1 : 0;
	};

	auto sum = [](const Matrix<int>& result)
	{
		return std::accumulate(result.elements.begin(), result.elements.end(), 0);
	};

	TransferMap map(size);

	auto computeRows = [&](unsigned begin, unsigned end)
	{
		for (unsigned y = begin; y < end; ++y)
		{
			for (unsigned x = 0; x < size.x; ++x)
			{
				Matrix<int> similarity = applyConvolution(_dst, makeKernel(x, y), distance, sum);

				// default guess is identity
				sf::Vector2u maxPos(x, y);
				int max = similarity(x, y);
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
	};

	utils::runMultiThreaded(0u, size.y, computeRows, _numThreads);

	return map;
}

TransferMap constructMapAvg(const sf::Image& _src, const sf::Image& _dst, unsigned _numThreads)
{
	const sf::Vector2u size = _src.getSize();

	const sf::Vector2u kernelSize(3, 3);
	const sf::Vector2i kernelHalf(kernelSize.x / 2, kernelSize.y / 2);
	Matrix<float> kernel(kernelSize, 1.f);
	kernel(1, 1) = 100.f;  // the center pixeel always takes precedence

	const float kernelSum = std::accumulate(kernel.elements.begin(), kernel.elements.end(), 0.f);

	auto sample = [](const float f, const sf::Color& color)
	{
		return f * toVec(color);
	};

	auto sum = [kernelSum](const Matrix<sf::Vector3f>& result)
	{
		return std::accumulate(result.elements.begin(), result.elements.end(), sf::Vector3f{})
			/ kernelSum;
	};

	TransferMap map(size);

	const Matrix<sf::Vector3f> dstAvg = math::applyConvolution(_dst, kernel, sample, sum);
	const Matrix<sf::Vector3f> srcAvg = math::applyConvolution(_src, kernel, sample, sum);

	auto computeRows = [&](unsigned begin, unsigned end)
	{
		for (unsigned y = begin; y < end; ++y)
		{
			for (unsigned x = 0; x < size.x; ++x)
			{
				sf::Vector2u pos(x, y);
				float min = math::distSq(dstAvg(x, y), srcAvg(x, y));
				for (size_t i = 0; i < dstAvg.elements.size(); ++i)
				{
					if (float d = math::distSq(dstAvg.elements[i], srcAvg(x, y)); d < min)
					{
						min = d;
						pos = dstAvg.index(i);
					}
				}
				map(x, y) = pos;
			}
		}
	};
	utils::runMultiThreaded(0u, size.y, computeRows);

	return map;
}

sf::Image distanceMap(const TransferMap& transferMap)
{
	sf::Image distImage;
	const sf::Vector2u size = transferMap.size;
	distImage.create(size.x, size.y);

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			sf::Color dist;
			const sf::Vector2u& v = transferMap(x, y) - sf::Vector2u(x, y);
			dist.g = static_cast<sf::Uint8>(std::abs(static_cast<float>(v.x) / size.x) * 255.f);
			dist.b = static_cast<sf::Uint8>(std::abs(static_cast<float>(v.y) / size.y) * 255.f);
			dist.a = 255;
			distImage.setPixel(x, y, dist);
		}
	}
	return distImage;
}

std::pair<sf::Uint8, sf::Uint8> minMaxBrightness(const sf::Image& _reference)
{
	const sf::Vector2u size = _reference.getSize();
	sf::Uint8 min = 255;
	sf::Uint8 max = 0;

	for (unsigned y = 0; y < size.y; ++y)
		for (unsigned x = 0; x < size.x; ++x)
		{
			const sf::Color color = _reference.getPixel(x, y);
			if (color.a == 0) continue;

			const sf::Uint8 avg = average(color);
			if (min > avg) min = avg;
			if (max < avg) max = avg;
		}

	return { min,max };
}

sf::Image colorMap(const TransferMap& transferMap, const sf::Image& _reference, bool _rgb)
{
	sf::Image prototypeImg;
	const sf::Vector2u size = transferMap.size;
	prototypeImg.create(size.x, size.y);

	auto [minCol, maxCol] = minMaxBrightness(_reference);
	const float range = static_cast<float>(maxCol - minCol);

	auto makeColorHSV = [=](const sf::Color& srcCol, float dx, float dy) 
	{
		const float v = (static_cast<int>(average(srcCol)) - minCol) / range;
		constexpr float t = 0.5f;
		const float dz = v >= 0.f ? v * t + 1.f - t : (1.f - t) * 0.5f;
		return HSVtoRGB(HSV{ dx * 360.f, dy, dz });
	};

	auto makeColorRGB = [=](const sf::Color& srcCol, float dx, float dy)
	{
		sf::Color color;
		color.g = static_cast<sf::Uint8>(dx * 255.f);
		color.b = static_cast<sf::Uint8>(dy * 255.f);
		color.r = average(srcCol);
		color.a = 255;
		return color;
	};

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			const sf::Color color = _reference.getPixel(x, y);
			const float dx = std::abs(static_cast<float>(x) / size.x);
			const float dy = std::abs(static_cast<float>(y) / size.y);

			prototypeImg.setPixel(x, y, _rgb ? makeColorHSV(color, dx,dy) 
				: makeColorRGB(color,dx,dy));
		}
	}

	sf::Image result = applyMap(transferMap, prototypeImg);
	return SpriteSheet({ std::move(prototypeImg), std::move(result) }).getCombined();
}

std::ostream& operator<<(std::ostream& _out, const TransferMap& _transferMap)
{
	_out << _transferMap.size.x << " " << _transferMap.size.y << "\n";
	for (unsigned y = 0; y < _transferMap.size.y; ++y)
	{
		for (unsigned x = 0; x < _transferMap.size.y; ++x)
		{
			const auto vec = _transferMap(x, y);
			_out << vec.x << " " << vec.y << "; ";
		}
		_out << "\n";
	}

	return _out;
}

std::istream& operator>>(std::istream& _in, TransferMap& _transferMap)
{
	sf::Vector2u vec;
	_in >> vec.x >> vec.y;
	_transferMap.resize(vec);

	for (auto& el : _transferMap.elements)
	{
		if (_in.eof())
		{
			std::cerr << "[Warning] Expected more elements in transfer map.\n";
			break;
		}

		char delim;
		_in >> vec.x >> vec.y >> delim;
		el = vec;
	}

	return _in;
}