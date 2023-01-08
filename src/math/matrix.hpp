#pragma once

#include <SFML/System/Vector2.hpp>
#include <cassert>
#include <iostream>
#include "../utils.hpp"

namespace math {
	// dynamic sized matrix
	template<typename T>
	struct Matrix
	{
		Matrix() = default;
		explicit Matrix(const sf::Vector2u& _size, const T& _default = {})
		{
			resize(_size, _default);
		}

		explicit Matrix(std::istream& _stream)
		{
			loadBinary(_stream);
		}

		void resize(const sf::Vector2u& _size, const T& _default = {})
		{
			size = _size;
			elements.resize(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), _default);
		}

		// direct access through flat index
		const T& operator[](size_t _flat) const { return elements[_flat]; }
		T& operator[](size_t _flat) { return elements[_flat]; }

		// access through column x row
		const T& operator()(unsigned x, unsigned y) const { return elements[flatIndex(x, y)]; }
		T& operator()(unsigned x, unsigned y) { return elements[flatIndex(x, y)]; }

		const T& operator()(const sf::Vector2u& _index) const { return elements[flatIndex(_index)]; }
		T& operator()(const sf::Vector2u& _index) { return elements[flatIndex(_index)]; }

		// 1D index of an element in the underlying array
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
				static_cast<unsigned>(flat / size.x));
		}

		auto begin() const { return elements.begin(); }
		auto end() const { return elements.end(); }
		auto begin() { return elements.begin(); }
		auto end() { return elements.end(); }

		// serialization
		template<typename Stream>
		void saveBinary(Stream& _stream) const
		{
			_stream.write(reinterpret_cast<const char*>(&size), sizeof(sf::Vector2u));
			_stream.write(reinterpret_cast<const char*>(elements.data()),
				elements.size() * sizeof(T));
		}

		// torch does not work with C++20 and also uses c10::utils somewhere causing ambiguity
		// with the namespace utils
		template<typename Stream, typename = std::enable_if_t<::utils::is_stream_writable<Stream,T>::value>>
	//	requires requires (T x) { std::declval<std::ofstream>() << x; }
		void save(Stream& _stream) const
		{
			_stream << size.x << " x " << size.y << "\n";

			for (unsigned y = 0; y < size.y; ++y)
			{
				for (unsigned x = 0; x < size.x; ++x)
				{
					_stream << (*this)(x, y) << " ";
				}
				_stream << ";\n";
			}
		}

		// stream needs to be open in binary mode!
		template<typename Stream>
		void loadBinary(Stream& _stream)
		{
			
			sf::Vector2u s;
			_stream.read(reinterpret_cast<char*>(&s), sizeof(sf::Vector2u));
			resize(s);
			_stream.read(reinterpret_cast<char*>(elements.data()), elements.size() * sizeof(T));
		}

		template<typename Stream, typename = std::enable_if_t<::utils::is_stream_readable<Stream, T>::value>>
		Stream& load(Stream& _in, T _default = {})
	//	requires requires (T x) { std::declval<std::ifstream>() >> x; }
		{
			sf::Vector2u vec;
			std::string delim;
			if (_in >> vec.x >> delim >> vec.y && delim == "x")
			{
				resize(vec, _default);

				for (unsigned y = 0; y < size.y; ++y)
				{
					for (unsigned x = 0; x < size.x; ++x)
					{
						T val;

						if (_in >> val)
							(*this)(x, y) = val;
						else
							return _in;
					}
					if (!(_in >> delim && delim == ";"))
						return _in;
				}
			}
			else
				std::cerr << "[Error] Could not read the size of the matrix.\n";

			return _in;
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
	Matrix<T>& operator*=(Matrix<T>& lhs, T rhs)
	{
		for (size_t i = 0; i < lhs.elements.size(); ++i)
			lhs[i] *= rhs;

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

	template<typename T>
	bool operator==(const Matrix<T>& lhs, const Matrix<T>& rhs)
	{
		if (lhs.size != rhs.size) return false;

		Matrix<T> result = lhs;
		for (size_t i = 0; i < lhs.elements.size(); ++i)
			if (lhs[i] != rhs[i])
				return false;

		return true;
	}

	template<typename T>
	Matrix<T> min(const Matrix<T>& lhs, const Matrix<T>& rhs)
	{
		assert(lhs.size == rhs.size);

		Matrix<T> result(lhs.size);
		for (size_t i = 0; i < lhs.elements.size(); ++i)
			result[i] = std::min(lhs[i], rhs[i]);
		return result;
	}

	template<typename T, typename = std::enable_if_t<utils::is_stream_writable<std::ostream, T>::value>>
//	requires requires (T x) { std::declval<std::ofstream>()  << x; }
	std::ostream& operator<<(std::ostream& _out, const Matrix<T>& _matrix)
	{
		_matrix.save(_out);
		return _out;
	}

	template<typename T, typename = std::enable_if_t<utils::is_stream_readable<std::istream, T>::value>>
//	requires requires (T x) { std::declval<std::ifstream>() >> x; }
	std::istream& operator>>(std::istream& _in, Matrix<T>& _matrix)
	{
		_matrix.load(_in);
		return _in;
	}
}