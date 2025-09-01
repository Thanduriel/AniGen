#include "pixelchains.hpp"
#include "../math/vectorext.hpp"
#include <unordered_set>
#include <array>

using PixelChain = std::deque<sf::Vector2u>;

using sf::Vector2u;

// @param _refMap A matrix of correct size to compute x,y coordinates of pixels.
PixelChain makePixelChain(const ZoneMap::PixelList& _pixels, 
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

		// todo: do swap erase since order is not important
		remainingPixels.erase(remainingPixels.begin() + idx);
	}

	return pixelChain;
}

TransferMap constructMap(const sf::Image& _referenceSprite, 
	const sf::Image& _targetSprite,
	const ZoneMap& _srcZoneMap,
	const ZoneMap& _dstZoneMap)
{
	const sf::Vector2u size = _referenceSprite.getSize();
	TransferMap transferMap(size);
	// init empty map
	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
			transferMap(x, y) = Vector2u(0, 0);
	}

	// iterate over zonemaps of src and dst simultaneously
	for (const auto& [zoneColor, dstPixels] : _dstZoneMap)
	{
		if (zoneColor == 0) 
			continue;

		const auto& srcPixels = _srcZoneMap(zoneColor);
		if (srcPixels.empty())
		{
			const sf::Color col(zoneColor);
			std::cout << "[Warning] Zone map is invalid. The zone color ("
				<< (int)col.r << ", " << (int)col.g << ", "
				<< (int)col.b << ", " << (int)col.a << ")"
				<< " was not found in the reference zone map. Skipping the current zone.\n";
			continue;
		}

		const PixelChain srcChain = makePixelChain(srcPixels, transferMap);
		PixelChain dstChain = makePixelChain(dstPixels, transferMap);

		// determine orientation by finding the closest front/back pair
		const std::array<size_t, 4> chainDistances = {
			math::distSq(srcChain.front(), dstChain.front()),
			math::distSq(srcChain.front(), dstChain.back()),
			math::distSq(srcChain.back(), dstChain.front()),
			math::distSq(srcChain.back(), dstChain.back())
		};
		auto minDist = std::min_element(chainDistances.begin(), chainDistances.end());
		auto minDistIdx = std::distance(chainDistances.begin(), minDist); 
		// reverse one chain to get same orientation in both
		if (minDistIdx == 1 || minDistIdx == 2)
			std::reverse(dstChain.begin(), dstChain.end());

		// Build map by walking the chains rescaled by the length ratio
		// to arrive at the end at the same time. We use int arithmetic
		// to avoid any rounding. +1 To reach the last pixel of the src chain.
		const size_t speed = srcChain.size() - 1;
		const size_t divisor = dstChain.size() -1;
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