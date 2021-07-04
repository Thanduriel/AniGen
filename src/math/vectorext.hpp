#pragma once

#include <SFML/System.hpp>

namespace math {
	inline sf::Vector3f toVec(const sf::Color& _color)
	{
		return sf::Vector3f(_color.r / 255.f, _color.g / 255.f, _color.b / 255.f);
	}

	// dot product
	template<typename T>
	T dot(const sf::Vector3<T>& a, const sf::Vector3<T>& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	// squared distance
	template<typename T>
	T distSq(const sf::Vector3<T>& a, const sf::Vector3<T>& b)
	{
		const sf::Vector3<T> dif = a - b;

		return dot(dif, dif);
	}
}