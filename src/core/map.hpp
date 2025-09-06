#pragma once

#include <SFML/Graphics.hpp>
#include "../math/matrix.hpp"
#include "../utils.hpp"

#include <unordered_map>

class ZoneMap
{
public:
	ZoneMap(const sf::Image& _src, const sf::Image& _dst, bool _ignoreAlpha = false);

	using PixelList = std::vector<size_t>;
	const PixelList& operator()(unsigned x, unsigned y) const;
	const PixelList& operator()(sf::Color _col) const;
	const PixelList& operator()(sf::Uint32 _col) const;

	auto begin() const { return m_srcZones.begin(); }
	auto end() const { return m_srcZones.end(); }

	const sf::Image& getDst() const { return m_dst; };
private:
	sf::Uint32 maskColor(sf::Uint32 _col) const;

	std::unordered_map<sf::Uint32, PixelList> m_srcZones;
	const sf::Image& m_dst;
	PixelList m_defaultZone; //< empty zone returned if a color does not exist in the reference
	sf::Uint32 m_ignoreMask;
};

// each element contains the coordinates for the source
using TransferMap = math::Matrix<sf::Vector2u>;

// Apply the map to a single image.
sf::Image applyMap(const TransferMap& _map, const sf::Image& _src);

// Extend a map with identity elements.
TransferMap extendMap(const TransferMap& _map, 
	const sf::Vector2u& _size, 
	const sf::Vector2u& _position);

/* Constructs a map that transforms _src to _dst.
 * A distance measure should be a functor with the signature
 *		Matrix<float> operator()(unsigned x, unsigned y)
 * that determines how similar (x,y) is to each pixel in the destination image,
 * where 0 is a perfect match. See pixelsimilarity.hpp for implementations.
 * @return The transfer map and a matrix with the final distance for each pixel.
 */
template<typename DistanceMeasure>
auto constructMap(const DistanceMeasure& _distanceMeasure,
	const ZoneMap* _zoneMap = nullptr,
	unsigned _numThreads = 1,
	sf::Vector2u _originOffset = {})
	-> std::pair<TransferMap, math::Matrix<float>>
{
	const sf::Vector2u size = _distanceMeasure.getSize();
	TransferMap map(size);
	math::Matrix<float> confidence(size);

	auto computeRows = [&](unsigned begin, unsigned end)
	{
		for (unsigned y = begin; y < end; ++y)
		{
			for (unsigned x = 0; x < size.x; ++x)
			{
				const auto distance = _distanceMeasure(x, y);
				// if the minimum is not unique prefer the identity
				size_t minInd = distance.flatIndex(x, y);

				if (_zoneMap)
				{
					const ZoneMap::PixelList& zone = (*_zoneMap)(x,y);
					if (zone.empty())
					{
						const sf::Color col = (*_zoneMap).getDst().getPixel(x, y);
						std::cout << "[Warning] Zone map is invalid. The color ("
							<< (int)col.r << ", " << (int)col.g << ", "
							<< (int)col.b << ", " << (int)col.a << ")"
							<< " at (" << _originOffset.x + x << ", " << _originOffset.y + y << ") does not exist in the reference.\n";
					}
					else
					{
						// identity is not part of the zone
						if (std::find(zone.begin(), zone.end(), minInd) == zone.end())
							minInd = zone.front();
						auto minEl = distance[minInd];
						for (size_t ind : zone)
							if (distance[ind] < minEl)
							{
								minEl = distance[ind];
								minInd = ind;
							}
					}
				}
				else
				{
					auto minEl = std::min_element(distance.begin(), distance.end());
					if (*minEl < distance(x, y))
						minInd = std::distance(distance.begin(), minEl);
				}

				map(x, y) = distance.index(minInd);
				confidence(x, y) = distance[minInd];
			}
		}
	};

	utils::runMultiThreaded(0u, size.y, computeRows, _numThreads);

	return { map, confidence };
}

// direct visualization of a TransferMap where distances are color coded
sf::Image distanceMap(const TransferMap& _transferMap);

// Create an image with a color gradient in both x and y direction such that each pixel
// has a unique color.
// @param _reference An image that determines the size and is integrated to be somewhat visible.
sf::Image makeColorGradientImage(const sf::Image& _reference, bool _rgb = true);
// visualize by applying the map to a high contrast image
sf::Image colorMap(const TransferMap& _transferMap, const sf::Image& _reference, bool _rgb = true);

// Encode matrix as gray scale image.
sf::Image matToImage(const math::Matrix<float>& _mat);

// serialization
std::ostream& operator<<(std::ostream& _out, const TransferMap& _transferMap);
std::istream& operator>>(std::istream& _in, TransferMap& _transferMap);