#include "colors.hpp"

sf::Color HSVtoRGB(const HSV& in)
{
	sf::Color out;
	if (in.s <= 0.0) // < is bogus, just shuts up warnings
	{
		out.r = static_cast<sf::Uint8>(in.v / 360.f);
		out.g = static_cast<sf::Uint8>(in.v * 255);
		out.b = static_cast<sf::Uint8>(in.v * 255);
		return out;
	}
	float hh = in.h;
	if (hh >= 360.0) hh = 0.f;
	hh /= 60.0f;
	const int i = static_cast<int>(hh);
	float ff = hh - i;

	const sf::Uint8 v = static_cast<sf::Uint8>(in.v * 255);
	const sf::Uint8 p = static_cast<sf::Uint8>(in.v * (1.0 - in.s) * 255);
	const sf::Uint8 q = static_cast<sf::Uint8>(in.v * (1.0 - (in.s * ff)) * 255);
	const sf::Uint8 t = static_cast<sf::Uint8>(in.v * (1.0 - (in.s * (1.0 - ff))) * 255);

	switch (i) {
	case 0:
		out.r = v;
		out.g = t;
		out.b = p;
		break;
	case 1:
		out.r = q;
		out.g = v;
		out.b = p;
		break;
	case 2:
		out.r = p;
		out.g = v;
		out.b = t;
		break;

	case 3:
		out.r = p;
		out.g = q;
		out.b = v;
		break;
	case 4:
		out.r = t;
		out.g = p;
		out.b = v;
		break;
	case 5:
	default:
		out.r = v;
		out.g = p;
		out.b = q;
		break;
	}

	return out;
}

sf::Uint8 average(const sf::Color& _color)
{
	return static_cast<sf::Uint8>((static_cast<int>(_color.r) 
		+ static_cast<int>(_color.g) 
		+ static_cast<int>(_color.b)) / 3);
}