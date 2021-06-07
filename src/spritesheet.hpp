#pragma once

#include <SFML/Graphics.hpp>

struct SpriteSheet
{
public:
	SpriteSheet(const std::string& _file, int _numFrames);
	SpriteSheet() = default;

	void save(const std::string& _file) const;

	std::vector<sf::Image> frames;
};