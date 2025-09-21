#include "spritesheet.hpp"

SpriteSheet::SpriteSheet(const std::string& _file, int _numFrames)
{
	sf::Image sprite;
	sprite.loadFromFile(_file);

	const sf::Vector2u frameSize = sf::Vector2u(sprite.getSize().x / _numFrames, sprite.getSize().y);

	frames.resize(_numFrames);
	for (int i = 0; i < _numFrames; ++i)
	{
		frames[i].create(frameSize.x, frameSize.y);
		frames[i].copy(sprite, 0, 0, sf::IntRect(frameSize.x * i, 0, frameSize.x, frameSize.y));
	}
}

std::pair<sf::Vector2u, sf::Vector2u> cropRect(const sf::Image& _image)
{
	const sf::Vector2u size = _image.getSize();
	sf::Vector2u min = size;
	sf::Vector2u max{};

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			if (_image.getPixel(x, y) != sf::Color::Transparent)
			{
				if (min.x > x) min.x = x;
				if (min.y > y) min.y = y;
				if (max.x < x) max.x = x;
				if (max.y < y) max.y = y;
			}
		}
	}

	return { min, max };
}

void SpriteSheet::crop(const sf::IntRect& _rect)
{
	for (auto& frame : frames)
	{
		frame = cropImage(frame, _rect);
	}
}

void SpriteSheet::applyZeroAlpha()
{
	for (sf::Image& img : frames)
		setZeroAlpha(img);
}

sf::Image SpriteSheet::getCombined() const
{
	const sf::Vector2u size = frames[0].getSize();
	const unsigned numFrames = static_cast<unsigned>(frames.size());
	sf::Image combined;
	combined.create(size.x * numFrames, size.y);
	for (unsigned i = 0; i < numFrames; ++i)
	{
		combined.copy(frames[i], i * size.x, 0);
	}

	return combined;
}

void SpriteSheet::save(const std::string& _file) const
{
	const sf::Image combined = getCombined();
	combined.saveToFile(_file);
}

// ************************************************************* //
sf::IntRect computeMinRect(std::vector<const sf::Image*> _sprites, unsigned _minBorder)
{
	const sf::Vector2u size = _sprites.front()->getSize();
	sf::Vector2u rectMin = size;
	sf::Vector2u rectMax{};

	for (auto& frame : _sprites)
	{
		auto [min, max] = cropRect(*frame);

		if (rectMin.x > min.x) rectMin.x = min.x;
		if (rectMin.y > min.y) rectMin.y = min.y;
		if (rectMax.x < max.x) rectMax.x = max.x;
		if (rectMax.y < max.y) rectMax.y = max.y;
	}

	const sf::Vector2u pos(rectMin.x > _minBorder ? rectMin.x - _minBorder : 0u, 
		rectMin.y > _minBorder ? rectMin.y - _minBorder : 0u);
	const sf::Vector2u posEnd = rectMax + sf::Vector2u(_minBorder, _minBorder);
	const sf::Vector2u newSize( std::min(posEnd.x,size.x) - pos.x, std::min(posEnd.y, size.y) - pos.y);
	
	return sf::IntRect(sf::Vector2i(pos), sf::Vector2i(newSize));
}

sf::IntRect computeMinRect(std::vector<const SpriteSheet*> _sheets, unsigned _minBorder)
{
	sf::Vector2u rectMin = _sheets.front()->frames.front().getSize();
	sf::Vector2u rectMax{};

	for (auto& sheet : _sheets)
	{
		for (const auto& frame : sheet->frames)
		{
			auto [min, max] = cropRect(frame);

			if (rectMin.x > min.x) rectMin.x = min.x;
			if (rectMin.y > min.y) rectMin.y = min.y;
			if (rectMax.x < max.x) rectMax.x = max.x;
			if (rectMax.y < max.y) rectMax.y = max.y;
		}
	}

	const sf::Vector2u pos(rectMin.x > _minBorder ? rectMin.x - _minBorder : 0u,
		rectMin.y > _minBorder ? rectMin.y - _minBorder : 0u);
	const sf::Vector2u newSize = rectMax - pos + sf::Vector2u(_minBorder, _minBorder);

	return sf::IntRect(sf::Vector2i(pos), sf::Vector2i(newSize));
}

sf::Image cropImage(const sf::Image& _img, const sf::IntRect& _rect)
{
	sf::Image croped;
	croped.create(_rect.width, _rect.height);
	croped.copy(_img, 0u, 0u, _rect);
	
	return croped;
}

void setZeroAlpha(sf::Image& _img)
{
	const sf::Vector2u size = _img.getSize();
	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			if (_img.getPixel(x, y).a == 0)
				_img.setPixel(x, y, sf::Color::Transparent);
		}
	}
}

sf::Color getPixelFlat(const sf::Image& _img, size_t _flat)
{
	const sf::Uint8* ptr = _img.getPixelsPtr() + 4 * _flat;
	return sf::Color(ptr[0], ptr[1], ptr[2], ptr[3]);
}

sf::Vector2u getIndex(const sf::Image& _img, size_t _flat)
{
	return sf::Vector2u(_flat % _img.getSize().x, _flat / _img.getSize().x);
}