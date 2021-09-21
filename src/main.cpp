#include <random>
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>
#include <filesystem>
#include <array>

#include <args.hxx>
#include "spritesheet.hpp"
#include "core/map.hpp"
#include "core/pixelsimilarity.hpp"
#include "colors.hpp"
#include "eval/eval.hpp"

using namespace math;

enum struct SimilarityType
{
	Identity,
	Equality,
	Blur,
	Count
};

const std::array<std::string, static_cast<size_t>(SimilarityType::Count)> SIMILARITY_TYPE_NAMES =
{ {
	{"identity"},
	{"equality"},
	{"blur"},
} };

std::pair< SimilarityType, Matrix<float>> parseSimilarityArg(const std::string& _arg)
{
	std::stringstream ss(_arg);

	const auto sizeSplit = _arg.find_last_of('x');
	std::string typeStr;
	ss >> typeStr;
	const auto typeIt = std::find(SIMILARITY_TYPE_NAMES.begin(), SIMILARITY_TYPE_NAMES.end(), typeStr);

	if (!ss || sizeSplit == std::string::npos || typeIt == SIMILARITY_TYPE_NAMES.end())
	{
		std::cerr << "[Warning] Could not parse the provided similarity_measure argument. Using defaults \"equlity_1x1\" instead.\n";
		return { SimilarityType::Identity, Matrix<float>(sf::Vector2u(1, 1)) };
	}

	Matrix<float> kernel;
	kernel.load(ss, 1.f);
	SimilarityType type = static_cast<SimilarityType>(std::distance(SIMILARITY_TYPE_NAMES.begin(), typeIt));
	// there is no difference if kernel size 1 is used
	if (kernel.size == sf::Vector2u(1,1) && type == SimilarityType::Equality) 
		type = SimilarityType::Identity;

	return { type, kernel };
}

int main(int argc, char* argv[])
{
	args::ArgumentParser parser("Sprite animation generator.");
	
	args::Group commands(parser, "commands");
	args::Command applyMode(commands, "apply", "apply an existing map to a sprite");
	args::Command createMode(commands, "create", "create a map from reference sprites");
	args::Command diffMode(commands, "difference", "compute difference between two sprites");
	
	args::Group arguments("arguments");
	args::HelpFlag help(arguments, "help", "display this help menu", { 'h', "help" });
	args::ValueFlag<int> framesInput(arguments, "num_input_frames", 
		"number of frames in the input sprites", { 'n', "input_frames" }, 1);
	args::ValueFlag<int> framesTarget(arguments, "num_target_frames", 
		"number of frames in the target sprites", { 'm', "target_frames" }, 1);
	args::ValueFlag<int> refFrame(arguments, "reference_frame", 
		"which frame to use as reference in the input sprites; only necessary if input_frames > 0", 
		{ 'r', "ref_frame" }, 0);
	args::ValueFlag<unsigned> threads(arguments, "threads", 
		"maximum number of threads to use", { 'j', "num_threads" }, 
		std::thread::hardware_concurrency());
	args::ValueFlagList<std::string> inputs(arguments, "inputs", 
		"the reference input sprites", { 'i', "inputs" });
	args::ValueFlagList<std::string> targets(arguments, "targets",
		"either the target sprites for the maps to (create) or the transfer maps to (apply)", 
		{ 't', "targets" });
	args::ValueFlag<std::string> outputName(arguments, "output", 
		"name for the map to (create); or the stem of the name for the sprite to create (apply), the name of the map and the ending is appended to this", 
		{ 'o', "output" });

	args::Flag zoneMapFlag(arguments, "zone_map",
		"for (create) use the first (input,target) pair as zone map instead as regular input", 
		{ 'z', "zones" });

	args::ValueFlag<std::string> similarityMeasure(arguments, "similarity_measure",
		"the similarity measure to use; either equality or blurred",
		{ 's', "similarity" }, "equality_1x1");
	args::Flag debugFlag(arguments, "debug", 
		"for (apply) the reference image is combined with a high contrast image to better visualize the map", 
		{ "debug" });
	args::GlobalOptions globals(parser, arguments);
	try
	{
		parser.ParseCLI(argc, argv);
	}
	catch (const args::Help&)
	{
		std::cout << parser;
		return 0;
	}
	catch (const args::ParseError& e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}
	catch (const args::ValidationError& e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}
	const unsigned numThreads = args::get(threads);

	auto start = std::chrono::high_resolution_clock::now();

	if (diffMode)
	{
		const auto inputNames = args::get(inputs);
		sf::Image imgA;
		imgA.loadFromFile(inputNames.front());
		setZeroAlpha(imgA);
		sf::Image imgB;
		imgB.loadFromFile(args::get(targets).front());
		setZeroAlpha(imgB);
		const sf::Image result = eval::imageDifference(imgA, imgB);
		const std::string name = outputName ? args::get(outputName) : "absDiff";
		result.saveToFile(name + ".png");
		
		std::cout << eval::relativeError(imgA, imgB, eval::PixelNeighbourhood(0));

		return 0;
	}

	// load reference sprites
	std::vector<sf::Image> referenceSprites;
	const auto inputNames = args::get(inputs);
	referenceSprites.reserve(inputNames.size());
	for (const auto& name : inputNames)
	{
		SpriteSheet sheet(name, args::get(framesInput));
		referenceSprites.emplace_back(sheet.frames[args::get(refFrame)]);
	}
	for (sf::Image& sprite : referenceSprites) setZeroAlpha(sprite);

	const auto targetNames = args::get(targets);

	if (createMode)
	{
		if (targetNames.size() != referenceSprites.size())
		{
			std::cerr << "[Error] The number of inputs (" << referenceSprites.size()
				<< ") and the number of targets (" << targetNames.size() << ") need to be equal.";
			return 1;
		}
		std::cout << "Building a transfer map from " << targetNames.size() 
			<< (targetNames.size() > 1 ? " samples.\n" : "sample.\n");

		// load targets
		const int numFrames = args::get(framesTarget);
		std::vector<SpriteSheet> targetSheets;
		targetSheets.reserve(targetNames.size());
		for (auto& name : targetNames)
		{
			targetSheets.emplace_back(name, numFrames);
			targetSheets.back().applyZeroAlpha();
		}

		// construct transfer maps and store them
		const std::string mapName = args::get(outputName);
		std::ofstream file(mapName);
		auto makeMaps = [&]<typename Similarity, bool WithId>(const Matrix<float>& _kernel)
		{
			for (int i = 0; i < numFrames; ++i)
			{
				using SimilarityT = std::conditional_t<WithId,
					MaskCompositionDistance<IdentityDistance, Similarity>,
					Similarity>;
				std::vector<SimilarityT> distances;
				// make zone map
				std::unique_ptr<ZoneMap> zoneMap;
				if (zoneMapFlag)
					zoneMap = std::make_unique<ZoneMap>(referenceSprites[0], targetSheets[0].frames[i]);

				for (size_t j = zoneMap ? 1 : 0; j < targetSheets.size(); ++j)
				{
					if constexpr (WithId)
						distances.emplace_back(IdentityDistance(referenceSprites[j], targetSheets[j].frames[i], _kernel),
							Similarity(referenceSprites[j], targetSheets[j].frames[i], _kernel));
					else
						distances.emplace_back(referenceSprites[j], targetSheets[j].frames[i], _kernel);
				}
				const TransferMap map = constructMap(GroupDistance(std::move(distances)), 
					zoneMap.get(),
					numThreads);

				file << map;
			}
		};
		auto [type, kernel] = parseSimilarityArg(args::get(similarityMeasure));
		switch (type)
		{
		case SimilarityType::Identity: makeMaps.operator()<IdentityDistance, false>(kernel);
			break;
		case SimilarityType::Equality: makeMaps.operator()<KernelDistance, false>(kernel);
			break;
		case SimilarityType::Blur: makeMaps.operator()<BlurDistance, false>(kernel);
			break;
		};

	}
	else if (applyMode)
	{
		// load transfer maps
		std::vector<std::vector<TransferMap>> transferMaps;
		transferMaps.reserve(targetNames.size());
		for (auto& name : targetNames)
		{
			if (!std::filesystem::exists(name))
			{
				std::cerr << "[Warning] Skipping the map "
					<< name << " as the file could not be found.\n";
				continue;
			}
			auto& sheetMaps = transferMaps.emplace_back();
			std::ifstream file(name);
			while (file)
			{
				sheetMaps.emplace_back();
				file >> sheetMaps.back();
			}
			// an empty map is read before encountering eof
			if (sheetMaps.back().size.x == 0)
				sheetMaps.pop_back();
		}

		if (debugFlag)
		{
			referenceSprites.front().saveToFile("reference1.png");
			for (auto& sprite : referenceSprites)
				sprite = makeColorGradientImage(sprite, true);
			referenceSprites.front().saveToFile("reference2.png");
		}

		std::cout << "Applying " << transferMaps.size() << " transfer map"
			<< (targetNames.size() != 1 ? "s." : ".") << "\n";

		for (size_t i = 0; i < referenceSprites.size(); ++i)
		{
			const sf::Image& reference = referenceSprites[i];

			for (size_t j = 0; j < transferMaps.size(); ++j)
			{
				auto& mapSheet = transferMaps[j];
				SpriteSheet sheet;
				for (TransferMap& map : mapSheet)
					sheet.frames.emplace_back(applyMap(map, reference));
				
				const std::filesystem::path mapName = targetNames[j];
				const std::string objectName = args::get(outputName);
				sheet.save(objectName + "_" + mapName.stem().string() + ".png");
			}
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "Computation took " << std::chrono::duration<double>(end - start).count() << "s." << std::endl;

	return 0;
}