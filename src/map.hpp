#pragma once

#include <SFML/Graphics.hpp>
#include "math/matrix.hpp"
#include "utils.hpp"

// each element contains the coordinates for the source
using TransferMap = math::Matrix<sf::Vector2u>;

sf::Image applyMap(const TransferMap& _map, const sf::Image& _src);

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
		const sf::Vector2u& _kernelSize = sf::Vector2u(3,3));

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

// constructs a map that transforms _src to _dst.
template<typename DistanceMeasure>
TransferMap constructMap(DistanceMeasure _distanceMeasure,
	unsigned _numThreads = 1)
{
	const sf::Vector2u size = _distanceMeasure.getSize();
	TransferMap map(size);

	auto computeRows = [&](unsigned begin, unsigned end)
	{
		for (unsigned y = begin; y < end; ++y)
		{
			for (unsigned x = 0; x < size.x; ++x)
			{
				const auto distance = _distanceMeasure(x, y);
				auto minEl = std::min_element(distance.elements.begin(), distance.elements.end());
				auto minInd = std::distance(distance.elements.begin(), minEl);

				// if the minimum is not unique prefer the identity
				map(x, y) = distance(x, y) == *minEl ? sf::Vector2u(x,y)
					: distance.index(minInd);
			}
		}
	};

	utils::runMultiThreaded(0u, size.y, computeRows, _numThreads);

	return map;
}

// direct visualisation of a TransferMap where distances are color coded
sf::Image distanceMap(const TransferMap& _transferMap);

// visualize by applying the map to a high constrast image
sf::Image colorMap(const TransferMap& _transferMap, const sf::Image& _reference, bool _rgb = true);

// serialization
std::ostream& operator<<(std::ostream& _out, const TransferMap& _transferMap);
std::istream& operator>>(std::istream& _in, TransferMap& _transferMap);

void saveBinary(const TransferMap& _transferMap);
TransferMap loadBinary(const std::string& _fileNmae);