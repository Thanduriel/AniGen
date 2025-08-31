#include "eval.hpp"
#include "../colors.hpp"

namespace eval {

	float PixelNeighbourhood::operator()(unsigned _x, unsigned _y, const sf::Image& _a, const sf::Image& _b)
	{
		const sf::Vector2u size = _a.getSize();
		const unsigned minY = _y > maxRadius ? _y - maxRadius : 0u;
		const unsigned minX = _x > maxRadius ? _x - maxRadius : 0u;
		const unsigned maxY = std::min(_y + maxRadius + 1, size.y);
		const unsigned maxX = std::min(_x + maxRadius + 1, size.x);

		for (unsigned y = minY; y < maxY; ++y)
			for (unsigned x = minX; x < maxX; ++x)
				if(_b.getPixel(_x,_y) == _a.getPixel(x,y))
					return 0.f;
		return 1.f;
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
				const sf::Color col1 = a.getPixel(x, y);
				const sf::Color col2 = b.getPixel(x, y);
				difImage.setPixel(x, y, absDist(col1, col2));
			}
		}
		return difImage;
	}
}