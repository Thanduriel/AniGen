#pragma once

#include "matrix.hpp"
#include <SFML/Graphics.hpp>

#include <functional>

// Access pixel x,y from _image. 
// If the coordinates lie outside the padded value is returned instead.
inline sf::Color getPixelPadded(const sf::Image& _image, int x, int y)
{
	if (x < 0 || y < 0
		|| x >= static_cast<int>(_image.getSize().x)
		|| y >= static_cast<int>(_image.getSize().y))
	{
		return sf::Color(0u);
	}

	return _image.getPixel(x, y);
}

namespace math {
	// perform a 2D convolution on an image
	template<typename T, typename Distance, typename Reduce>
	auto applyConvolution(const sf::Image& _image, const Matrix<T>& _kernel,
		Distance _dist, Reduce _reduce)
	{
		using ReturnType = decltype(std::function{ _reduce })::result_type;
		using DistanceType = decltype(std::function{ _dist })::result_type;

		const sf::Vector2u size = _image.getSize();
		const sf::Vector2u kernelHalf(_kernel.size.x / 2, _kernel.size.y / 2);

		// explicit padding
		sf::Image paddedImg;
		paddedImg.create(size.x + _kernel.size.x-1, size.y + _kernel.size.y - 1);
		paddedImg.copy(_image, kernelHalf.x, kernelHalf.y);
		const sf::Uint8* pixels = paddedImg.getPixelsPtr();

		// result matrix can be reused
		Matrix<DistanceType> kernelResult(_kernel.size);
		auto computeKernel = [&](unsigned x, unsigned y)
		{
			for (unsigned i = 0; i < _kernel.size.y; ++i)
			{
				const unsigned yInd = (y + i - kernelHalf.y);
				for (unsigned j = 0; j < _kernel.size.x; ++j)
				{
					const unsigned xInd = x + j - kernelHalf.x;
					const unsigned kernelInd = j + i * _kernel.size.x;
					kernelResult[kernelInd] = _dist(_kernel[kernelInd],
						paddedImg.getPixel(xInd, yInd));
				}
			}
			return _reduce(kernelResult);
		};

		Matrix<ReturnType> result(_image.getSize());

		for (unsigned y = 0; y < size.y; ++y)
			for (unsigned x = 0; x < size.x; ++x)
			{
				result(x, y) = computeKernel(x+kernelHalf.x, y+kernelHalf.y);
			}

		return result;
	}
}