#pragma once

#include <SFML/Graphics.hpp>
#include "math/matrix.hpp"
#include "utils.hpp"

class DistanceBase
{
public:
	DistanceBase(const sf::Image& _src, const sf::Image& _dst);

	sf::Vector2u getSize() const { return m_src.getSize(); }
protected:
	const sf::Image& m_src;
	const sf::Image& m_dst;
};

class KernelDistance : public DistanceBase
{
public:
	KernelDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const sf::Vector2u& _kernelSize = sf::Vector2u(3, 3));

	math::Matrix<float> operator()(unsigned x, unsigned y) const;
private:
	math::Matrix<sf::Color> makeKernel(unsigned x, unsigned y) const;

	sf::Vector2u m_kernelSize;
	sf::Vector2u m_kernelHalSize;
};

class BlurDistance : public DistanceBase
{
public:
	BlurDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const sf::Vector2u& _kernelSize = sf::Vector2u(3, 3));

	math::Matrix<float> operator()(unsigned x, unsigned y) const;
private:
	math::Matrix<sf::Vector3f> m_dstBlurred;
	math::Matrix<sf::Vector3f> m_srcBlurred;
	sf::Vector2u m_kernelHalSize;
};

template<typename... DistanceMeasures>
class SumDistance
{
public:
	SumDistance(DistanceMeasures&&... _distanceMeasures)
		: m_distances(std::forward<DistanceMeasures>(_distanceMeasures)...)
	{}

	math::Matrix<float> operator()(unsigned x, unsigned y) const
	{
		return evaluate(x, y, std::make_index_sequence<sizeof...(DistanceMeasures)>{});
	}

	sf::Vector2u getSize() const { return std::get<0>(m_distances).getSize(); }
private:
	template<std::size_t... I>
	math::Matrix<float> evaluate(unsigned x, unsigned y, std::index_sequence<I...>) const
	{
		return (... + std::get<I>(m_distances)(x, y));
	}

	std::tuple<DistanceMeasures...> m_distances;
};

// sum but for a single type
template<typename DistanceMeasure>
class GroupDistance
{
public:
	GroupDistance(std::vector<DistanceMeasure>&& _distanceMeasures)
		: m_distances(std::move(_distanceMeasures))
	{}

	math::Matrix<float> operator()(unsigned x, unsigned y) const
	{
		math::Matrix<float> distance = m_distances[0](x, y);
		for (size_t i = 1; i < m_distances.size(); ++i)
			distance += m_distances[i](x, y);

		return distance;
	}

	sf::Vector2u getSize() const { return m_distances.front().getSize(); }
private:

	std::vector<DistanceMeasure> m_distances;
};