#pragma once

#include <SFML/System/Vector2.hpp>
#include <cassert>
#include <iostream>

namespace math {
	// dynamic sized matrix
	template<typename T>
	struct Matrix
	{
		Matrix() = default;
		Matrix(const sf::Vector2u& _size, const T& _default = {})
		{
			resize(_size, _default);
		}

		explicit Matrix(std::istream& _stream)
		{
			load(_stream);
		}

		void resize(const sf::Vector2u& _size, const T& _default = {})
		{
			size = _size;
			elements.resize(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), _default);
		}

		// direct access through flat index
		const T& operator[](size_t _flat) const { return elements[_flat]; }
		T& operator[](size_t _flat) { return elements[_flat]; }

		const T& operator()(unsigned x, unsigned y) const { return elements[flatIndex(x, y)]; }
		T& operator()(unsigned x, unsigned y) { return elements[flatIndex(x, y)]; }

		const T& operator()(const sf::Vector2u& _index) const { return elements[flatIndex(_index)]; }
		T& operator()(const sf::Vector2u& _index) { return elements[flatIndex(_index)]; }


		size_t flatIndex(unsigned x, unsigned y) const
		{
			return x + static_cast<size_t>(y) * size.x;
		}
		size_t flatIndex(const sf::Vector2u& _index) const
		{
			return _index.x + static_cast<size_t>(_index.y) * size.x;
		}

		sf::Vector2u index(size_t flat) const
		{
			return sf::Vector2u(static_cast<unsigned>(flat % size.x),
				static_cast<unsigned>(flat / size.y));
		}

		auto begin() const { return elements.begin(); }
		auto end() const { return elements.end(); }
		auto begin() { return elements.begin(); }
		auto end() { return elements.end(); }

		// binary serialization
		template<typename Stream>
		void save(Stream& _stream) const
		{
			_stream.write(reinterpret_cast<const char*>(&size), sizeof(sf::Vector2u));
			_stream.write(reinterpret_cast<const char*>(elements.data()),
				elements.size() * sizeof(T));
		}

		// stream needs to be open in binary mode!
		template<typename Stream>
		void load(Stream& _stream)
		{
			
			sf::Vector2u s;
			_stream.read(reinterpret_cast<char*>(&s), sizeof(sf::Vector2u));
			resize(s);
			_stream.read(reinterpret_cast<char*>(elements.data()), elements.size() * sizeof(T));
		}

		sf::Vector2u size;
		std::vector<T> elements;
	};

	template<typename T>
	Matrix<T> operator+(const Matrix<T>& lhs, const Matrix<T>& rhs)
	{
		assert(lhs.size == rhs.size);

		Matrix<T> result = lhs;
		for (size_t i = 0; i < lhs.elements.size(); ++i)
			result[i] += rhs[i];

		return result;
	}

	template<typename T>
	Matrix<T>& operator+=(Matrix<T>& lhs, const Matrix<T>& rhs)
	{
		assert(lhs.size == rhs.size);

		for (size_t i = 0; i < lhs.elements.size(); ++i)
			lhs[i] += rhs[i];

		return lhs;
	}

	template<typename T>
	Matrix<T> operator-(const Matrix<T>& lhs, const Matrix<T>& rhs)
	{
		assert(lhs.size == rhs.size);

		Matrix<T> result = lhs;
		for (size_t i = 0; i < lhs.elements.size(); ++i)
			result[i] -= rhs[i];

		return result;
	}

}