#include <SFML/System.hpp>

namespace math {
	// dot product
	template<typename T>
	T dot(const sf::Vector3<T>& a, const sf::Vector3<T>& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	template<typename T>
	T distSq(const sf::Vector3<T>& a, const sf::Vector3<T>& b)
	{
		const sf::Vector3<T> dif = a - b;

		return dot(dif, dif);
	}
}