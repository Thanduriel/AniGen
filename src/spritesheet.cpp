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
		//	image.saveToFile("frame" + std::to_string(i) + ".png");
	}
}

void SpriteSheet::save(const std::string& _file) const
{
	const sf::Vector2u size = frames[0].getSize();
	const unsigned numFrames = static_cast<unsigned>(frames.size());
	sf::Image combined;
	combined.create(size.x * numFrames, size.y);
	for (unsigned i = 0; i < numFrames; ++i)
	{
		combined.copy(frames[i], i * size.x, 0);
	}
	combined.saveToFile(_file);
}