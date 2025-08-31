#pragma once

#include "map.hpp"
#include "../spritesheet.hpp"

#include <SFML/System/Vector2.hpp>
#include <deque>
#include <vector>

TransferMap constructMap(const sf::Image& _referenceSprite, 
	const sf::Image& _targetSprite,
	const ZoneMap& _srcZoneMap,
	const ZoneMap& _dstZoneMap);