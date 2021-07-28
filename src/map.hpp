#pragma once

#include <SFML/Graphics.hpp>
#include "math/matrix.hpp"
#include "utils.hpp"

// each element contains the coordinates for the source
using TransferMap = math::Matrix<sf::Vector2u>;

// Apply the map to a single image.
sf::Image applyMap(const TransferMap& _map, const sf::Image& _src);

/* Constructs a map that transforms _src to _dst.
 * A distance measure should be a functor with the signature
 *		Matrix<float> operator()(unsigned x, unsigned y)
 * that determines how similar (x,y) is to each pixel in the destination image,
 * where 0 is a perfect match. See pixelsimilarity.hpp for implementations.
 */
template<typename DistanceMeasure>
TransferMap constructMap(const DistanceMeasure& _distanceMeasure,
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
				auto minEl = std::min_element(distance.begin(), distance.end());
				auto minInd = std::distance(distance.begin(), minEl);

				// if the minimum is not unique prefer the identity
				map(x, y) = (distance(x, y) == *minEl) ? 
					sf::Vector2u(x,y) : distance.index(minInd);
			}
		}
	};

	utils::runMultiThreaded(0u, size.y, computeRows, _numThreads);

	return map;
}

// direct visualization of a TransferMap where distances are color coded
sf::Image distanceMap(const TransferMap& _transferMap);

// visualize by applying the map to a high contrast image
sf::Image colorMap(const TransferMap& _transferMap, const sf::Image& _reference, bool _rgb = true);

// serialization
std::ostream& operator<<(std::ostream& _out, const TransferMap& _transferMap);
std::istream& operator>>(std::istream& _in, TransferMap& _transferMap);