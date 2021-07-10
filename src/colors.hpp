#pragma once

#include <SFML/Graphics.hpp>

struct HSV
{
	float h;       // angle in degrees
	float s;       // a fraction between 0 and 1
	float v;       // a fraction between 0 and 1
};

sf::Color HSVtoRGB(const HSV& _color);

sf::Uint8 average(const sf::Color& _color);