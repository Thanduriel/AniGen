#pragma once

#include <SFML/Graphics.hpp>

namespace eval {

	sf::Image imageDifference(const sf::Image& a, const sf::Image& b);

	struct PixelNeighbourhood
	{
		PixelNeighbourhood(unsigned _maxRadius = 0) : maxRadius(_maxRadius) {}

		unsigned maxRadius;
		
		float operator()(unsigned _x, unsigned _y, const sf::Image& _a, const sf::Image& _b);
	};
	// Computes a single value for the difference between _a and _b
	// where 0 is equality.
	template<typename Diff>
	float relativeError(const sf::Image& _a, const sf::Image& _b, Diff _diff)
	{
		const sf::Vector2u size = _a.getSize();
		float err = 0.f;

		for (unsigned y = 0; y < size.y; ++y)
		{
			for (unsigned x = 0; x < size.x; ++x)
			{
				err += _diff(x, y, _a, _b);
			}
		}
		return err / (size.x * size.y);
	}
}