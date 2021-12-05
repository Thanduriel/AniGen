#pragma once

#include <SFML/Graphics.hpp>
#include "../math/matrix.hpp"
#include "../utils.hpp"

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
		const math::Matrix<float>& = {});

	math::Matrix<float> operator()(unsigned x, unsigned y) const;
};

class KernelDistance : public DistanceBase
{
public:
	// Create a kernel distance measure with constant weights.
	KernelDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const sf::Vector2u& _kernelSize = sf::Vector2u(3, 3),
		float _rotation = 0.f);

	KernelDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const math::Matrix<float>& _kernel,
		float _rotation = 0.f);

	math::Matrix<float> operator()(unsigned x, unsigned y) const;

	const math::Matrix<sf::Vector2i>& sampleCoords() const { return m_sampleCoords; }
private:
	using Kernel = math::Matrix<std::pair<float, sf::Color>>;
	Kernel makeKernel(unsigned x, unsigned y) const;

	sf::Vector2u m_kernelHalSize;
	math::Matrix<float> m_kernelWeights;
	math::Matrix<sf::Vector2i> m_sampleCoords;
	float m_kernelSum;
};

class BlurDistance : public DistanceBase
{
public:
	BlurDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const sf::Vector2u& _kernelSize = sf::Vector2u(3, 3));

	BlurDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const math::Matrix<float>& _kernel);

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

		distance *= 1.f / m_distances.size();

		return distance;
	}

	sf::Vector2u getSize() const { return m_distances.front().getSize(); }
private:
	std::vector<DistanceMeasure> m_distances;
};

template<typename DistanceMeasure>
class GroupDistanceThreshold
{
public:
	explicit GroupDistanceThreshold(std::vector<DistanceMeasure>&& _distanceMeasures,
			float _threshold = 1.f)
		: m_distances(std::move(_distanceMeasures)),
		m_discardThreshold(_threshold)
	{}

	math::Matrix<float> operator()(unsigned x, unsigned y) const
	{
		std::vector<math::Matrix<float>> dists;
		dists.reserve(m_distances.size());
		dists.emplace_back(m_distances[0](x, y));
		auto distSum = dists.front();

		for (size_t i = 1; i < m_distances.size(); ++i)
		{
			dists.emplace_back(m_distances[i](x, y));
			distSum += dists.back();
		}

		for (size_t i = 0; i < dists.front().elements.size(); ++i)
		{
			size_t numMeasures = m_distances.size();
			const float threshold = distSum[i] / numMeasures + m_discardThreshold;
			for (auto& d : dists)
			{
				if (d[i] > threshold)
				{
					distSum[i] -= d[i];
					--numMeasures;
				}
			}
			distSum[i] *= 1.f / numMeasures;
		}

		return distSum;
	}

	sf::Vector2u getSize() const { return m_distances.front().getSize(); }
private:
	float m_discardThreshold;
	std::vector<DistanceMeasure> m_distances;
};

template<typename DistanceMeasure>
class GroupMinDistance
{
public:
	explicit GroupMinDistance(std::vector<DistanceMeasure>&& _distanceMeasures)
		: m_distances(std::move(_distanceMeasures))
	{}

	math::Matrix<float> operator()(unsigned x, unsigned y) const
	{
		math::Matrix<float> distance = m_distances[0](x, y);
		for (size_t i = 1; i < m_distances.size(); ++i)
			distance = math::min(distance, m_distances[i](x, y));

		return distance;
	}

	sf::Vector2u getSize() const { return m_distances.front().getSize(); }
private:
	std::vector<DistanceMeasure> m_distances;
};

class RotInvariantKernelDistance : public GroupMinDistance<KernelDistance>
{
public:
	RotInvariantKernelDistance(const sf::Image& _src,
		const sf::Image& _dst,
		const math::Matrix<float>& _kernel);
private:
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