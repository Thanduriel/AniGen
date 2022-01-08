#include <random>
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>
#include <filesystem>
#include <array>
#include <concepts>

#include <args.hxx>
#include "spritesheet.hpp"
#include "core/map.hpp"
#include "core/pixelsimilarity.hpp"
#include "colors.hpp"
#include "eval/eval.hpp"
#ifdef WITH_TORCH
#include "core/optim.hpp"
#endif

using namespace math;

enum struct SimilarityType
{
	Identity,
	Equality,
	Blur,
	EqualityRotInv,
	MinEquality,
	MinBlur,
	MinEqualityRotInv,
#ifdef WITH_TORCH
	MSEOptim,
#endif
	COUNT
};

const std::array<std::string, static_cast<size_t>(SimilarityType::COUNT)> SIMILARITY_TYPE_NAMES =
{ {
	{"identity"},
	{"equality"},
	{"blur"},
	{"equalityrotinv"},
	{"minequality"},
	{"minblur"},
	{"minequalityrotinv"},
#ifdef WITH_TORCH
	{"mseoptim"}
#endif
} };

constexpr const char* defaultSimilarity = "equality 3 x 3 1 1 1; 1 1 1; 1 1 1;";

std::pair< SimilarityType, Matrix<float>> parseSimilarityArg(const std::string& _arg)
{
	std::stringstream ss(_arg);

	const auto sizeSplit = _arg.find_last_of('x');
	std::string typeStr;
	ss >> typeStr;
	const auto typeIt = std::find(SIMILARITY_TYPE_NAMES.begin(), SIMILARITY_TYPE_NAMES.end(), typeStr);

	if (!ss || sizeSplit == std::string::npos || typeIt == SIMILARITY_TYPE_NAMES.end())
	{
		std::cerr << "[Warning] Could not parse the provided similarity_measure argument. Using defaults \""
			<< defaultSimilarity << "\" instead.\n";
		return parseSimilarityArg(std::string(defaultSimilarity));
	}

	Matrix<float> kernel;
	kernel.load(ss, 1.f);
	SimilarityType type = static_cast<SimilarityType>(std::distance(SIMILARITY_TYPE_NAMES.begin(), typeIt));
	// there is no difference if kernel size 1 is used
	if (kernel.size == sf::Vector2u(1,1) && type == SimilarityType::Equality) 
		type = SimilarityType::Identity;

	return { type, kernel };
}

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
	std::ofstream& file;
	bool debugFlag;
	std::vector<sf::Image>& confidenceImgs;

	template<typename Similarity, template<typename> class Group, bool WithId>
	void run(const Matrix<float>& _kernel)
	{
		for (int i = 0; i < numFrames; ++i)
		{
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
					distances.emplace_back(IdentityDistance(referenceSprites[j], targetSheets[j].frames[i], _kernel),
						Similarity(referenceSprites[j], targetSheets[j].frames[i], _kernel));
				else
					distances.emplace_back(referenceSprites[j], targetSheets[j].frames[i], _kernel);
			}

			auto constructGroupSim = [&]()
			{
				if constexpr (std::is_constructible_v<GroupSimilarity, std::vector<SimilarityT>, float>)
					return GroupSimilarity(std::move(distances), discardThreshold);
				else
					return GroupSimilarity(std::move(distances));
			};

			auto [map, confidence] = constructMap(constructGroupSim(),
				zoneMap.get(),
				numThreads);

			if (minBorder)
				map = extendMap(map, originalSize, originalPosition);

			file << map;

			if (debugFlag)
				confidenceImgs.emplace_back(matToImage(confidence));
		}
	}
};

int main(int argc, char* argv[])
{
	args::ArgumentParser parser("Sprite animation generator.");
	
	args::Group commands(parser, "commands");
	args::Command applyMode(commands, "apply", "apply an existing map to a sprite");
	args::Command createMode(commands, "create", "create a map from reference sprites");
	args::Command diffMode(commands, "evaluate", "compute difference between two sprites");
	
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
		"a string describing the similarity measure to use for map creation; general form: \"type a x b m11 m21 ...; m21 m22 ...; ...\"",
		{ 's', "similarity" }, defaultSimilarity);
	args::Flag debugFlag(arguments, "debug", 
		"during (create) additional information is output; for (apply) the reference image is combined with a high contrast image to better visualize the map", 
		{ "debug" });
	args::ValueFlag<int> cropMinBorder(arguments, "crop_border",
		"accelerate (create) by cropping the empty frame around each sprite to at most crop_border pixels",
		{ "crop" }, 0);
	args::ValueFlag<float> discardTreshold(arguments, "discard_threshold",
		"discards distance values during (create) with multiple sprites if are larger mean + threshold; distance values are in the range [0,1]",
		{ "threshold" }, 1.0);

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
		
		std::cout << eval::relativeError(imgA, imgB, eval::PixelNeighbourhood(0))
			<< ", " << eval::relativeError(imgA, imgB, eval::PixelNeighbourhood(1));

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
				<< ") and the number of targets (" << targetNames.size() << ") need to be equal.\n";
			return 1;
		}
		std::cout << "Building a transfer map from " << targetNames.size() 
			<< (targetNames.size() > 1 ? " samples.\n" : " sample.\n");

		// load targets
		const int numFrames = args::get(framesTarget);
		std::vector<SpriteSheet> targetSheets;
		targetSheets.reserve(targetNames.size());
		for (auto& name : targetNames)
		{
			targetSheets.emplace_back(name, numFrames);
			targetSheets.back().applyZeroAlpha();
		}

		const sf::Vector2u originalSize = referenceSprites.front().getSize();
		sf::Vector2u originalPosition;
		const int minBorder = args::get(cropMinBorder);
		if (minBorder)
		{
			std::vector<const sf::Image*> sprites;
			for (const auto& sprite : referenceSprites)
				sprites.push_back(&sprite);
			for (const SpriteSheet& sheet : targetSheets)
				for (const sf::Image& sprite : sheet.frames)
					sprites.push_back(&sprite);
			sf::IntRect minRect = computeMinRect(sprites, minBorder);
			originalPosition = sf::Vector2u(minRect.left, minRect.top);

			for (sf::Image& sprite : referenceSprites)
				sprite = cropImage(sprite, minRect);
			for (SpriteSheet& sheet : targetSheets)
				sheet.crop(minRect);
		}

		// construct transfer maps and store them
		const std::string mapName = args::get(outputName);
		std::ofstream file(mapName);
		std::vector<sf::Image> confidenceImgs;

		auto [type, kernel] = parseSimilarityArg(args::get(similarityMeasure));

		MapMaker maker{ zoneMapFlag, 
			numFrames, 
			referenceSprites, 
			targetSheets, 
			args::get(discardTreshold),
			numThreads,
			minBorder,
			originalSize,
			originalPosition,
			file,
			debugFlag,
			confidenceImgs};

		switch (type)
		{
		case SimilarityType::Identity: maker.run<IdentityDistance, GroupDistanceThreshold, false>(kernel);
			break;
		case SimilarityType::Equality: maker.run<KernelDistance, GroupDistanceThreshold, false>(kernel);
			break;
		case SimilarityType::Blur: maker.run<BlurDistance, GroupDistanceThreshold, false>(kernel);
			break;
		case SimilarityType::EqualityRotInv:maker.run<RotInvariantKernelDistance, GroupDistanceThreshold, false > (kernel);
			break;
		case SimilarityType::MinEquality: maker.run<KernelDistance, GroupMinDistance, false>(kernel);
			break;
		case SimilarityType::MinBlur: maker.run<BlurDistance, GroupMinDistance, false>(kernel);
			break;
		case SimilarityType::MinEqualityRotInv:maker.run< RotInvariantKernelDistance, GroupMinDistance, false > (kernel);
			break;
#ifdef WITH_TORCH
		case SimilarityType::MSEOptim:
			for (size_t i = 0; i < numFrames; ++i)
			{
				std::vector<sf::Image> dstImages;
				dstImages.reserve(targetSheets.size());
				for (auto& sheet : targetSheets)
					dstImages.push_back(sheet.frames[i]);
				auto map = nn::constructMapOptim(referenceSprites, dstImages);
				file << map;
			}
			break;
#endif
		};

		if (debugFlag)
		{
			SpriteSheet sheet(std::move(confidenceImgs));
			sheet.save("confidence.png");
		}

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