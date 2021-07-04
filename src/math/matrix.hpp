#pragma once

namespace math {
	// dynamic sized matrix
	template<typename T>
	struct Matrix
	{
		Matrix() = default;
		Matrix(const sf::Vector2u& _size, const T& _default = {}) : size(_size)
		{
			elements.resize(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), _default);
		}

		const T& operator()(unsigned x, unsigned y) const { return elements[flatIndex(x, y)]; }
		T& operator()(unsigned x, unsigned y) { return elements[flatIndex(x, y)]; }

		size_t flatIndex(unsigned x, unsigned y) const
		{
			return x + static_cast<size_t>(y) * size.x;
		}

		sf::Vector2u index(size_t flat) const
		{
			return sf::Vector2u(static_cast<unsigned>(flat % size.x),
				static_cast<unsigned>(flat / size.y));
		}

		sf::Vector2u size;
		std::vector<T> elements;
	};

}