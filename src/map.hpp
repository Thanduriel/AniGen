#pragma once

#include <SFML/Graphics.hpp>
#include "math/matrix.hpp"

using TransferMap = math::Matrix<sf::Vector2u>;
sf::Image applyMap(const TransferMap& _map, const sf::Image& _src);

// constructs a map that transforms _src to _dst.
TransferMap constructMap(const sf::Image& _src, const sf::Image& _dst, unsigned _numThreads = 1);

// based on average surrounding pixel values
TransferMap constructMapAvg(const sf::Image& _src, const sf::Image& _dst, unsigned _numThreads = 1);

// direct visualisation of a TransferMap wher distances are color coded
sf::Image distanceMap(const TransferMap& _transferMap);

// visualize by applying the map to a high constrast map
sf::Image colorMap(const TransferMap& _transferMap);