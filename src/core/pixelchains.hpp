#pragma once

#include "map.hpp"
#include "../spritesheet.hpp"

#include <SFML/System/Vector2.hpp>
#include <deque>
#include <vector>
#include <array>

enum struct OrientationHeuristic 
{
	MinDistance,
	Direction,
	COUNT
};

const std::array<std::string, static_cast<size_t>(OrientationHeuristic::COUNT)> ORIENTATION_HEURISTICS_NAMES =
{ {
	{"mindist"},
	{"direction"},
} };

TransferMap constructMap(const sf::Image& _referenceSprite, 
	const sf::Image& _targetSprite,
	const ZoneMap& _srcZoneMap,
	const ZoneMap& _dstZoneMap,
	OrientationHeuristic _orientationHeuristic);