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

		const sf::Vector2i kernelHalf(_kernel.size.x / 2, _kernel.size.y / 2);
		auto computeKernel = [&](unsigned x, unsigned y)
		{
			Matrix<DistanceType> result(_kernel.size);
			for (unsigned i = 0; i < _kernel.size.y; ++i)
			{
				const int yInd = static_cast<int>(y + i) - kernelHalf.y;
				for (unsigned j = 0; j < _kernel.size.x; ++j)
				{
					// use padding if index is negative
					const int xInd = static_cast<int>(x + j) - kernelHalf.x;
					result(j, i) = _dist(_kernel.elements[j + i * _kernel.size.x],
						getPixelPadded(_image, xInd, yInd));
				}
			}
			return _reduce(result);
		};

		Matrix<ReturnType> result(_image.getSize());

		const sf::Vector2u size = _image.getSize();
		for (unsigned y = 0; y < size.y; ++y)
			for (unsigned x = 0; x < size.x; ++x)
			{
				result(x, y) = computeKernel(x, y);
			}

		return result;
	}
}