#include <SFML/Graphics.hpp>
#include <random>
#include <functional>
#include <thread>
#include <chrono>
#include <iostream>
#include <numeric>

#include "spritesheet.hpp"
#include "convolution.hpp"
#include "vectorext.hpp"

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

// It must be random access for now
template<typename It, typename Fn>
void runMultiThreaded(It _begin, It _end, Fn _fn, size_t _numThreads = 1)
{
	if (_numThreads == 1)
		_fn(_begin, _end);
	else
	{
		std::vector<std::thread> threads;
		threads.reserve(_numThreads - 1);

		using DistanceType = decltype(_end - _begin);
		const DistanceType n = static_cast<DistanceType>(_numThreads);
		const DistanceType rows = (_end -_begin) / n;
		for (DistanceType i = 0; i < n - 1; ++i)
			threads.emplace_back(_fn, i * rows, (i + 1) * rows);
		_fn((n - 1) * rows, _end);

		for (auto& thread : threads)
			thread.join();
	}
}


// constructs a map that transforms _src to _dst.
TransferMap constructMap(const sf::Image& _src, const sf::Image& _dst, unsigned _numThreads = 1)
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
				kernel(j,i) = getPixelPadded(_src, xInd, yInd);
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

	runMultiThreaded(0u, size.y, computeRows, 4);

	return map;
}

TransferMap constructMapAvg(const sf::Image& _src, const sf::Image& _dst, unsigned _numThreads = 1)
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

	const Matrix<sf::Vector3f> dstAvg = applyConvolution(_dst, kernel, sample, sum);
	const Matrix<sf::Vector3f> srcAvg = applyConvolution(_src, kernel, sample, sum);

	auto computeRows = [&](unsigned begin, unsigned end)
	{
		for (unsigned y = begin; y < end; ++y)
		{
			for (unsigned x = 0; x < size.x; ++x)
			{
				sf::Vector2u pos(x, y);
				float min = math::distSq(dstAvg(x,y), srcAvg(x,y));
				for (size_t i = 0; i < dstAvg.elements.size(); ++i)
				{
					if (float d = math::distSq(dstAvg.elements[i], srcAvg(x,y)); d < min)
					{
						min = d;
						pos = dstAvg.index(i);
					}
				}
				map(x, y) = pos;
			}
		}
	};
	runMultiThreaded(0u, size.y, computeRows);

	return map;
}

// compute the L1 difference between a and b
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

// direct visualisation of a TransferMap
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
			const sf::Vector2u& v = transferMap(x, y) - sf::Vector2u(x,y);
			dist.g = static_cast<sf::Uint8>(std::abs(static_cast<float>(v.x) / size.x) * 255.f);
			dist.b = static_cast<sf::Uint8>(std::abs(static_cast<float>(v.y) / size.y) * 255.f);
			dist.a = 255;
			distImage.setPixel(x, y, dist);
		}
	}
	return distImage;
}

int main()
{
	auto start = std::chrono::high_resolution_clock::now();
	SpriteSheet reference("../Sprites/HumanMale_ApronChest.png", 8);
	SpriteSheet validation("../Sprites/HumanMale_FamilyChest_Medium.png", 8);

	SpriteSheet output;
	SpriteSheet dif;
	output.frames.emplace_back(validation.frames[0]);
	dif.frames.emplace_back(imageDifference(validation.frames[0], output.frames[0]));
	for (int i = 1; i < 8; ++i)
	{
		const TransferMap map = constructMap(reference.frames[0], reference.frames[i], 4);
		distanceMap(map).saveToFile("distance" + std::to_string(i) + ".png");
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

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration<double>(end - start).count() << std::endl;

	return 0;
}