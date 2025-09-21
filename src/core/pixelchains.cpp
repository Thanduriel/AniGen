#include "pixelchains.hpp"
#include "../math/vectorext.hpp"
#include "../utils/colors.hpp"
#include <unordered_set>
#include <array>
#include <optional>

using sf::Vector2u;
// use deque because we build chains starting from a random point
using PixelChain = std::deque<Vector2u>;

// @param _refMap A matrix of correct size to compute x,y coordinates of pixels.
PixelChain makePixelChain(const PixelList& _pixels, 
	const TransferMap& _refMap)
{
	std::vector<Vector2u> remainingPixels;
	remainingPixels.reserve(_pixels.size());
	for (size_t p : _pixels)
		remainingPixels.emplace_back(_refMap.index(p));

	PixelChain pixelChain {remainingPixels.back()};
	remainingPixels.pop_back();

	std::vector<size_t> frontDists;
	std::vector<size_t> backDists;

	auto updateDists = [&](Vector2u o, std::vector<size_t>& dists) 
	{
		dists.clear();
		for (Vector2u p : remainingPixels)
			dists.push_back(math::distSq(o, p));
	};

	while (!remainingPixels.empty()) 
	{
		updateDists(pixelChain.front(), frontDists);
		updateDists(pixelChain.back(), backDists);

		auto frontDistIt = std::min_element(frontDists.begin(), frontDists.end());
		auto backDistIt = std::min_element(backDists.begin(), backDists.end());

		size_t idx = 0;
		if (*frontDistIt <= *backDistIt){
			idx = std::distance(frontDists.begin(), frontDistIt);
			pixelChain.push_front(remainingPixels[idx]);
		} else {
			idx = std::distance(backDists.begin(), backDistIt);
			pixelChain.push_back(remainingPixels[idx]);
		}

		// remove pixel by moving it to the end since the order does not matter
		remainingPixels[idx] = remainingPixels.back();
		remainingPixels.pop_back();
	}

	return pixelChain;
}

// Marks the start pixel of a chain.
constexpr sf::Uint8 START_ALPHA = 155;
// Looks for a start marker and reverses the chain if necessary.
// @return True if the chain (now) starts with the marked pixel.
bool ensureOrientation(PixelChain& _pixelChain, const sf::Image& _sprite)
{
	auto startIt = _pixelChain.end();
	for (auto it = _pixelChain.begin(); it != _pixelChain.end(); ++it)
	{
		if (const sf::Color col = _sprite.getPixel(it->x, it->y); col.a == START_ALPHA)
		{
			// already found one
			if (startIt != _pixelChain.end()) 
			{
				std::cout << "[Warning] Encountered multiple starting points in a chain with color ("
					<< col << "). First marker is at " << startIt->x << ", " << startIt->y
					<< ". Additional marker found at " << it->x << ", " << it->y << ".\n";
			}
			else
				startIt = it;
		}
	}
	// not found
	if (startIt == _pixelChain.end())
	{
	//	std::cout << "[Warning] No start marker found.\n";
		return false;
	}
	// found but orientation is wrong
	if (startIt == _pixelChain.end() - 1)
		std::reverse(_pixelChain.begin(), _pixelChain.end());
	else if (startIt != _pixelChain.begin()) // found but invalid
	{
		const sf::Color col = _sprite.getPixel(startIt->x, startIt->y);
		std::cout << "[Warning] Pixel chain start marker is not at one of the ends in a chain with color ("
					<< col << "). The marker was found at " << startIt->x << ", " << startIt->y << ".\n";
		return false;
	}
	return true;
}

TransferMap constructMap(const sf::Image& _referenceSprite, 
	const sf::Image& _targetSprite,
	const ZoneMap& _srcZoneMap,
	const ZoneMap& _dstZoneMap,
	OrientationHeuristic _orientationHeuristic,
	ErrorImageWrapper& _errorRefImage,
	ErrorImageWrapper& _errorTargetImage)
{
	const sf::Vector2u size = _referenceSprite.getSize();
	// init empty map
	TransferMap transferMap(size, Vector2u(0, 0));

	// iterate over zonemaps of src and dst simultaneously
	for (const auto& [zoneColor, dstPixels] : _dstZoneMap)
	{
		// ignore the empty exterior
		if (zoneColor == 0) 
			continue;

		const sf::Color col(zoneColor);

		const auto& srcPixels = _srcZoneMap(zoneColor);
		if (srcPixels.empty())
		{
			std::cout << "[Warning] Zone map is invalid. The zone color (" << col
				<< ") (alpha channel is ignored) was not found in the reference zone map. Skipping the current zone.\n";
			_errorTargetImage.draw(dstPixels, col);
			continue;
		}

		PixelChain srcChain = makePixelChain(srcPixels, transferMap);
		PixelChain dstChain = makePixelChain(dstPixels, transferMap);

		const bool srcIsMarked = ensureOrientation(srcChain, _referenceSprite);
		const bool dstIsMarked = ensureOrientation(dstChain, _targetSprite);

		// ensureOrientation can also return false if no markers found which
		// is currently not treated as an error. Therefore also check if there
		// are marks available.
		if (!srcPixels.marks.empty() && !srcIsMarked)
			_errorRefImage.draw(srcPixels, col, true);
		if (!dstPixels.marks.empty() && !srcIsMarked)
			_errorTargetImage.draw(dstPixels, col, true);

		// at least one chain does not have a start marker
		if (!srcIsMarked || !dstIsMarked)
		{
			if (!srcIsMarked && dstIsMarked)
			{
				std::cout << "[Warning] Found a chain start marker in the target but not in the source zone with color (" << col 
					<< ") (alpha channel is ignored).\n";
				_errorRefImage.draw(srcPixels, col, true);
				_errorTargetImage.draw(dstPixels, col, true);
			}

			bool orientationMatch = true;
			switch (_orientationHeuristic)
			{
			case OrientationHeuristic::MinDistance:
			{
				// determine orientation by finding the closest front/back pair
				const std::array<size_t, 4> chainDistances = {
					math::distSq(srcChain.front(), dstChain.front()),
					math::distSq(srcChain.front(), dstChain.back()),
					math::distSq(srcChain.back(), dstChain.front()),
					math::distSq(srcChain.back(), dstChain.back())
				};
				const auto minDist = std::min_element(chainDistances.begin(), chainDistances.end());
				const auto minDistIdx = std::distance(chainDistances.begin(), minDist); 
				// the closest ends of the chains already match
				orientationMatch = minDistIdx == 0 || minDistIdx == 3;
				break;
			}
			case OrientationHeuristic::Direction:
			{
				const Vector2u v1 = srcChain.front() - srcChain.back();
				const Vector2u v2 = dstChain.front() - dstChain.back();

				// current orientation is in the same direction
				orientationMatch = math::dot(v1, v2) > 0;
				break;
			}
			default:
				std::cerr << "[Error] Invalid orientation heuristic.\n";
				std::abort();
			};
			// reverse one chain to get same orientation in both
			if (!orientationMatch)
				std::reverse(dstChain.begin(), dstChain.end());
		}

		// Build map by walking the chains rescaled by the length ratio
		// to arrive at the end at the same time. We use int arithmetic
		// to avoid any rounding. +1 To reach the last pixel of the src chain.
		const size_t speed = srcChain.size() - 1;
		// Prevent division by 0. In that case we only do a single iteration
		// so the actual value does not matter beyond being non-zero.
		const size_t divisor = dstChain.size() > 1 ? dstChain.size() - 1 : 1;
		size_t srcProgress = 0;
		auto srcIt = srcChain.begin();
		for (auto dstPixel : dstChain)
		{
			transferMap(dstPixel) = *srcIt;
			
			srcProgress += speed;
			const size_t advanceSteps = srcProgress / divisor;
			srcProgress %= divisor;
			// going past the post end iterator is undefined
			const size_t endDiff = std::distance(srcIt, srcChain.end());
			srcIt += std::min(advanceSteps, endDiff);
		}
	}

	return transferMap;
}