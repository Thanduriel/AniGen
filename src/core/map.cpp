#include "map.hpp"
#include "../utils.hpp"
#include "../spritesheet.hpp"
#include "../colors.hpp"

#include <assert.h>
#include <iostream>

using namespace math;

ZoneMap::ZoneMap(const sf::Image& _src, const sf::Image& _dst)
	: m_dst(_dst)
{
	const sf::Vector2u size = _src.getSize();
	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			m_srcZones[_src.getPixel(x, y).toInteger()].push_back(x + y * size.x);
		}
	}
}

const ZoneMap::PixelList& ZoneMap::operator()(unsigned x, unsigned y) const
{
	auto it = m_srcZones.find(m_dst.getPixel(x, y).toInteger());
	return it != m_srcZones.end() ? it->second : m_defaultZone;
}

const ZoneMap::PixelList& ZoneMap::operator()(sf::Color _color) const
{
	auto it = m_srcZones.find(_color.toInteger());
	return it != m_srcZones.end() ? it->second : m_defaultZone;
}

const ZoneMap::PixelList& ZoneMap::operator()(sf::Uint32 _color) const
{
	auto it = m_srcZones.find(_color);
	return it != m_srcZones.end() ? it->second : m_defaultZone;
}

// ************************************************************* //
sf::Image applyMap(const TransferMap& _map, const sf::Image& _src)
{
	sf::Image image;
	image.create(_src.getSize().x, _src.getSize().y, sf::Color::Black);
	for (unsigned y = 0; y < image.getSize().y; ++y)
	{
		const unsigned idy = y * image.getSize().x;
		for (unsigned x = 0; x < image.getSize().x; ++x)
		{
			const sf::Vector2u& srcPos = _map[x + idy];
			image.setPixel(x, y, _src.getPixel(srcPos.x, srcPos.y));
		}
	}
	return image;
}

// ************************************************************* //
sf::Image distanceMap(const TransferMap& transferMap)
{
	sf::Image distImage;
	const sf::Vector2u size = transferMap.size;
	distImage.create(size.x, size.y);

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			sf::Color dist;
			const sf::Vector2u& v = transferMap(x, y) - sf::Vector2u(x, y);
			dist.g = static_cast<sf::Uint8>(std::abs(static_cast<float>(v.x) / size.x) * 255.f);
			dist.b = static_cast<sf::Uint8>(std::abs(static_cast<float>(v.y) / size.y) * 255.f);
			dist.a = 255;
			distImage.setPixel(x, y, dist);
		}
	}
	return distImage;
}

TransferMap extendMap(const TransferMap& _map,
	const sf::Vector2u& _size,
	const sf::Vector2u& _position)
{
	TransferMap result(_size);
	for (unsigned y = 0; y < _size.y; ++y)
		for (unsigned x = 0; x < _size.x; ++x)
			result(x, y) = sf::Vector2u(x, y);

	for (unsigned y = 0; y < _map.size.y; ++y)
		for (unsigned x = 0; x < _map.size.x; ++x)
			result(x + _position.x, y + _position.y) = _map(x, y) + _position;

	return result;
}

std::pair<sf::Uint8, sf::Uint8> minMaxBrightness(const sf::Image& _reference)
{
	const sf::Vector2u size = _reference.getSize();
	sf::Uint8 min = 255;
	sf::Uint8 max = 0;

	for (unsigned y = 0; y < size.y; ++y)
		for (unsigned x = 0; x < size.x; ++x)
		{
			const sf::Color color = _reference.getPixel(x, y);
			if (color.a == 0) continue;

			const sf::Uint8 avg = average(color);
			if (min > avg) min = avg;
			if (max < avg) max = avg;
		}

	return { min,max };
}

sf::Image makeColorGradientImage(const sf::Image& _reference, bool _rgb)
{
	sf::Image prototypeImg;
	const sf::Vector2u size = _reference.getSize();
	prototypeImg.create(size.x, size.y);

	auto [minCol, maxCol] = minMaxBrightness(_reference);
	const float range = static_cast<float>(maxCol - minCol);

	auto makeColorHSV = [=](const sf::Color& srcCol, float dx, float dy)
	{
		const float v = (static_cast<int>(average(srcCol)) - minCol) / range;
		constexpr float t = 0.5f;
		const float dz = v >= 0.f ? v * t + 1.f - t : (1.f - t) * 0.5f;
		return HSVtoRGB(HSV{ dx * 360.f, dy, dz });
	};

	auto makeColorRGB = [=](const sf::Color& srcCol, float dx, float dy)
	{
		sf::Color color;
		color.g = static_cast<sf::Uint8>(dx * 255.f);
		color.b = static_cast<sf::Uint8>(dy * 255.f);
		color.r = average(srcCol);
		color.a = static_cast<sf::Uint8>(255);
		return color;
	};

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			const sf::Color color = _reference.getPixel(x, y);
			const float dx = std::abs(static_cast<float>(x) / size.x);
			const float dy = std::abs(static_cast<float>(y) / size.y);

		//	prototypeImg.setPixel(x, y, color);
			prototypeImg.setPixel(x, y, _rgb ? makeColorRGB(color, dx, dy)
				: makeColorHSV(color, dx, dy));
		}
	}

	return prototypeImg;
}

sf::Image colorMap(const TransferMap& transferMap, const sf::Image& _reference, bool _rgb)
{
	sf::Image prototypeImg = makeColorGradientImage(_reference, _rgb);
	sf::Image result = applyMap(transferMap, prototypeImg);
	return SpriteSheet({ std::move(prototypeImg), std::move(result) }).getCombined();
}

sf::Image matToImage(const Matrix<float>& _mat)
{
	sf::Image img;
	auto maxIt = std::max_element(_mat.begin(), _mat.end());
	const float max = *maxIt > 0.f ? std::max(1.f, *maxIt) : 1.f;

	img.create(_mat.size.x, _mat.size.y);
	for (unsigned y = 0; y < _mat.size.y; ++y)
		for (unsigned x = 0; x < _mat.size.x; ++x)
		{
			const sf::Uint8 c = static_cast<sf::Uint8>(255.f * _mat(x, y) / max);
			img.setPixel(x, y, sf::Color(c, c, c));
		}

	return img;
}

// ************************************************************* //
std::ostream& operator<<(std::ostream& _out, const TransferMap& _transferMap)
{
	_out << _transferMap.size.x << " " << _transferMap.size.y << "\n";
	for (unsigned y = 0; y < _transferMap.size.y; ++y)
	{
		for (unsigned x = 0; x < _transferMap.size.x; ++x)
		{
			const auto vec = _transferMap(x, y);
			_out << vec.x << " " << vec.y << ", ";
		}
		_out << "\n";
	}

	return _out;
}

std::istream& operator>>(std::istream& _in, TransferMap& _transferMap)
{
	sf::Vector2u vec;
	if (_in >> vec.x >> vec.y)
	{
		_transferMap.resize(vec);

		for (auto& el : _transferMap.elements)
		{
			if (_in.eof())
			{
				std::cerr << "[Warning] Expected more elements in transfer map.\n";
				break;
			}

			char delim;
			_in >> vec.x >> vec.y >> delim;
			el = vec;
		}
	}

	return _in;
}