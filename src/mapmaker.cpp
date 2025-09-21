#include "mapmaker.hpp"

void MapMaker::runChains()
{
	// input validation
	if (!zoneMapFlag)
	{
		std::cerr << "[Error] Distance measure \"chains\" requires a zone map consisting of single pixel lines.\n";
		std::abort();
	}
	if (referenceSprites.size() > 1 || targetSheets.size() > 1)
	{
		std::cout << "[Warning] Redundant inputs for distance measure \"chains\". Only the zone map is currently used in this mode.\n";
	}

	const auto orientationHeuristic = static_cast<OrientationHeuristic>(kernel.size.x);

	auto makeFn = [&](int i) 
	{
		std::cout << "Creating map for frame " << i << "...\n";

			// create zonemaps for both sprites
		const ZoneMap srcZoneMap(referenceSprites[0], targetSheets[0].frames[i], true);
		const ZoneMap dstZoneMap(targetSheets[0].frames[i], referenceSprites[0], true);

		// we dont use the actual target
		TransferMap map = constructMap(referenceSprites[0], 
			targetSheets[0].frames[i],
			srcZoneMap,
			dstZoneMap,
			orientationHeuristic);

		return map;
	};

	makeForEachFrame(makeFn);
}