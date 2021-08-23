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

// 0 if pixels have equal color; 1 if not
class IdentityDistance : public DistanceBase
{
public:
	// the last argument is just a dummy
	IdentityDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const sf::Vector2u& = sf::Vector2u(1,1));

	math::Matrix<float> operator()(unsigned x, unsigned y) const;
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
		: m_distances(std::move<DistanceMeasures>(_distanceMeasures)...)
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
	explicit GroupDistance(std::vector<DistanceMeasure>&& _distanceMeasures)
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

// composition of two distances that returns DistMeasure2 if DistMeasure1 is 0
// and max_float otherwise.
template<typename DistMeasure1, typename DistMeasure2>
class MaskCompositionDistance
{
public:
	MaskCompositionDistance(DistMeasure1&& _distMeasure1, DistMeasure2&& _distMeasure2, float _maxDist = 10000.f)
		: m_mask(std::move(_distMeasure1)),
		m_distance(std::move(_distMeasure2)),
		m_maxDistance(_maxDist)
	{
		assert(m_mask.getSize() == m_distance.getSize());
	}

	math::Matrix<float> operator()(unsigned x, unsigned y) const
	{
		const math::Matrix<float> mask = m_mask(x, y);
		math::Matrix<float> dist = m_distance(x, y);

		for (size_t i = 0; i < mask.elements.size(); ++i)
		{
			if (mask[i] != 0.f)
				dist[i] = m_maxDistance;
		}

		return dist;
	}

	sf::Vector2u getSize() const { return m_distance.getSize(); }
private:
	DistMeasure1 m_mask;
	DistMeasure2 m_distance;
	float m_maxDistance;
};