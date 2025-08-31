#include "colors.hpp"
#include <cmath>

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
	const float hh = std::fmod(in.h, 360.f) / 60.f;
	const int i = static_cast<int>(hh);
	const float ff = hh - i;

	const sf::Uint8 v = static_cast<sf::Uint8>(in.v * 255);
	const sf::Uint8 p = static_cast<sf::Uint8>(in.v * (1.f - in.s) * 255);
	const sf::Uint8 q = static_cast<sf::Uint8>(in.v * (1.f - (in.s * ff)) * 255);
	const sf::Uint8 t = static_cast<sf::Uint8>(in.v * (1.f - (in.s * (1.f - ff))) * 255);

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

sf::Color absDist(const sf::Color& _a, const sf::Color& _b)
{
	sf::Color dif;

	dif.r = std::abs(static_cast<int>(_a.r) - static_cast<int>(_b.r));
	dif.g = std::abs(static_cast<int>(_a.g) - static_cast<int>(_b.g));
	dif.b = std::abs(static_cast<int>(_a.b) - static_cast<int>(_b.b));
	dif.a = 255;

	return dif;
}