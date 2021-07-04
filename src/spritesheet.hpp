#pragma once

#include <SFML/Graphics.hpp>

struct SpriteSheet
{
public:
	// load image and split into _numFrames equal sizes frames in x dimension
	SpriteSheet(const std::string& _file, int _numFrames);
	SpriteSheet() = default;
	SpriteSheet(std::vector<sf::Image> _frames) : frames(std::move(_frames)) {}

	void crop(const sf::IntRect& _rect);

	// get single image containing all frames
	sf::Image getCombined() const;
	// combines frames and saves them as a single image
	void save(const std::string& _file) const;

	std::vector<sf::Image> frames;
};

// determine minimum rectangle that contains all nonzero pixels over all sheets
sf::IntRect computeMinRect(std::vector<SpriteSheet*> _sheets, unsigned _minBorder = 0);