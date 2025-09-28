#pragma once

#include <fstream>
#include <iostream>

#include <vector>
#include "core/map.hpp"
#include "core/pixelsimilarity.hpp"
#include "core/pixelchains.hpp"
#include "utils/spritesheet.hpp"

// Functor which holds all the processing state.
struct MapMaker
{
	bool zoneMapFlag;
	int numFrames;
	std::vector<sf::Image>& referenceSprites;
	std::vector<SpriteSheet>& targetSheets;
	float discardThreshold;
	unsigned numThreads;
	int minBorder;
	sf::Vector2u originalSize;
	sf::Vector2u originalPosition;
	std::string mapName;
	std::ofstream& file;
	bool debugFlag;
	std::vector<sf::Image>& confidenceImgs;
	const math::Matrix<float>& kernel;
	float chainMaxTimeInSec;

	// run with pixel chains
	void runChains();

	// run with color similarity search
	template<typename Similarity, template<typename> class Group, typename MakeSimilarity = int, bool WithId = false>
	void run(const MakeSimilarity& _othSimilarity = 0)
	{
		auto makeFn = [&](int i, ErrorImageWrapper& errorRefImage, ErrorImageWrapper& errorTargetImage) {
			using SimilarityT = std::conditional_t<WithId,
				MaskCompositionDistance<IdentityDistance, Similarity>,
				Similarity>;
			using GroupSimilarity = Group<Similarity>;
			std::vector<SimilarityT> distances;
			// make zone map
			std::unique_ptr<ZoneMap> zoneMap;
			if (zoneMapFlag)
				zoneMap = std::make_unique<ZoneMap>(referenceSprites[0], targetSheets[0].frames[i]);

			for (size_t j = zoneMap ? 1 : 0; j < targetSheets.size(); ++j)
			{
				if constexpr (WithId)
					distances.emplace_back(IdentityDistance(referenceSprites[j], targetSheets[j].frames[i], kernel),
						Similarity(referenceSprites[j], targetSheets[j].frames[i], kernel));
				else
					distances.emplace_back(referenceSprites[j], targetSheets[j].frames[i], kernel);
			}

			auto constructGroupSim = [&]()
			{
				if constexpr (std::is_constructible_v<GroupSimilarity, std::vector<SimilarityT>, float>)
					return GroupSimilarity(std::move(distances), discardThreshold);
				else
					return GroupSimilarity(std::move(distances));
			};
			auto constructFullSim = [&]()
			{
				if constexpr (std::is_same_v<MakeSimilarity, int>)
					return constructGroupSim();
				else
					return SumDistance(constructGroupSim(), _othSimilarity(i));
			};

			auto [map, confidence] = constructMap(constructFullSim(),
				zoneMap.get(),
				numThreads,
				originalPosition);

			if (debugFlag)
				confidenceImgs.emplace_back(matToImage(confidence));

			return map;
		};

		makeForEachFrame(makeFn);
	}

private:
	// create map for each frame
	template<typename MakeFn>
	void makeForEachFrame(MakeFn _makeFn)
	{
		const sf::Vector2u s = referenceSprites.front().getSize();
		sf::Image errorRefImg;
		errorRefImg.create(s.x, s.y, sf::Color(0,0,0,0));
		ErrorImageWrapper errorRefWrapper(errorRefImg);

		SpriteSheet errorSheet;
		errorSheet.frames.resize(numFrames, errorRefImg);

		bool highlightedErrors = false;

		for (int i = 0; i < numFrames; ++i)
		{
			std::cout << "Creating map for frame " << i << "...\n";

			ErrorImageWrapper errorTargetWrapper(errorSheet.frames[i]);

			TransferMap map = _makeFn(i, errorRefWrapper, errorTargetWrapper);

			if (minBorder)
				map = extendMap(map, originalSize, originalPosition);
			file << map;

			highlightedErrors |= !errorTargetWrapper.isEmpty();
		}

		if (!errorRefWrapper.isEmpty())
		{
			errorRefImg = extendImage(errorRefImg, originalSize, originalPosition);
			const std::string fileName = mapName + ".ref_issues.png";
			std::cout << "Issues where found in the reference input. Creating \"" << fileName << "\" to highlight them.\n";
			errorRefImg.saveToFile(fileName);
		}

		if (highlightedErrors)
		{
			errorSheet.extend(originalSize, originalPosition);
			
			const std::string fileName = mapName + ".target_issues.png";
			std::cout << "Issues where found in the target inputs. Creating \"" << fileName << "\" to highlight them.\n";
			errorSheet.save(fileName);
		}
	}
};