#pragma once

#include <SFML/Graphics.hpp>

struct HSV
{
	float h;	// hue - angle in degrees
	float s;	// saturation - a fraction between 0 and 1
	float v;	// brightness - a fraction between 0 and 1
};

sf::Color HSVtoRGB(const HSV& _color);

sf::Uint8 average(sf::Color _color);

sf::Color absDist(sf::Color _a, sf::Color _b);

std::ostream& operator<<(std::ostream& _out, sf::Color _color);