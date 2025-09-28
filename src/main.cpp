#include <random>
#include <chrono>
#include <thread>
#include <filesystem>
#include <array>

#include <args.hxx>
#include "mapmaker.hpp"
#include "utils/colors.hpp"
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
	Chain,
#ifdef WITH_TORCH
	MSEOptim,
	EqualityMSEOptim,
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
	{"chain"},
#ifdef WITH_TORCH
	{"mseoptim"},
	{"equality_mseoptim"}
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

	if (typeIt == SIMILARITY_TYPE_NAMES.end())
	{
		std::cerr << "[Error] Could not parse the provided similarity_measure argument. Uknown similarity measure "
			<< "\"" << typeStr << "\"\n";
		std::abort();
	}

	Matrix<float> kernel;
	SimilarityType type = static_cast<SimilarityType>(std::distance(SIMILARITY_TYPE_NAMES.begin(), typeIt));

	// does not need a kernel
	if (type == SimilarityType::Chain)
	{
		std::string orientationString;
		ss >> orientationString;
		if (orientationString.empty())
			orientationString = "direction";
		const auto orientationIt = std::find(ORIENTATION_HEURISTICS_NAMES.begin(), ORIENTATION_HEURISTICS_NAMES.end(), orientationString);

		if (orientationIt == ORIENTATION_HEURISTICS_NAMES.end())
		{
			std::cerr << "[Error] The provided orientation metric \""
				<< orientationString << "\" is unknown.\n";
			std::abort();
		}
		kernel.resize(sf::Vector2u(std::distance(ORIENTATION_HEURISTICS_NAMES.begin(), orientationIt), 0));

		return {type, kernel};
	}

	// parse kernel specification
	if (!ss || sizeSplit == std::string::npos)
	{
		std::cerr << "[Error] Could not parse the provided similarity_measure argument. Using defaults \""
			<< defaultSimilarity << "\" instead.\n";
		std::abort();
	}

	kernel.load(ss, 1.f);
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
	args::Command diffMode(commands, "evaluate", "compute difference between two sprites");
	
	args::Group createArgs("creation exclusive arguments");
	args::Command createMode(commands, "create", "create a map from reference sprites",
		[&](args::Subparser& subParser)
		{
			subParser.Add(createArgs);
			subParser.Parse();
		});

	args::Group arguments("arguments");
	args::HelpFlag help(arguments, "help", "display this help menu", { 'h', "help" });
	args::ValueFlag<int> framesInput(arguments, "num_input_frames", 
		"number of frames in the input sprites", { 'n', "input_frames" }, 1);
	args::ValueFlag<int> framesTarget(createArgs, "num_target_frames", 
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

	args::Flag zoneMapFlag(createArgs, "zone_map",
		"for (create) use the first (input,target) pair as zone map instead of as regular input", 
		{ 'z', "zones" });

	args::ValueFlag<std::string> similarityMeasure(createArgs, "similarity_measure",
		"a string describing the similarity measure to use for map creation; general form: \"type a x b m11 m21 ...; m21 m22 ...; ...\"",
		{ 's', "similarity" }, defaultSimilarity);
	args::Flag debugFlag(arguments, "debug", 
		"during (create) additional information is output; for (apply) the reference image is combined with a high contrast image to better visualize the map", 
		{ "debug" });
	args::ValueFlag<int> cropMinBorder(createArgs, "crop_border",
		"accelerate (create) by cropping the empty frame around each sprite to at most crop_border pixels",
		{ "crop" }, 0);
	args::ValueFlag<float> discardTreshold(createArgs, "discard_threshold",
		"discards distance values during (create) with multiple sprites if they are larger than mean + threshold; distance values are in the range [0,1]",
		{ "threshold" }, 1.f);
	
	args::ValueFlag<float> chainMaxTimeInSec(createArgs, "chain_search_time",
		"maximum time in [s] to search for optimal chain during (create); special values: \"0\" - no search, use greedy algorithm instead; <0 - no time limit",
		{ "chain_search_time" }, 1.f);

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
		return 1;
	}
	catch (const args::ValidationError& e)
	{
		std::cerr << e.what() << std::endl;
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
			mapName,
			file,
			debugFlag,
			confidenceImgs,
			kernel,
			args::get(chainMaxTimeInSec)};

		switch (type)
		{
		case SimilarityType::Identity: maker.run<IdentityDistance, GroupDistanceThreshold>();
			break;
		case SimilarityType::Equality: maker.run<KernelDistance, GroupDistanceThreshold>();
			break;
		case SimilarityType::Blur: maker.run<BlurDistance, GroupDistanceThreshold>();
			break;
		case SimilarityType::EqualityRotInv:maker.run<RotInvariantKernelDistance, GroupDistanceThreshold>();
			break;
		case SimilarityType::MinEquality: maker.run<KernelDistance, GroupMinDistance>();
			break;
		case SimilarityType::MinBlur: maker.run<BlurDistance, GroupMinDistance>();
			break;
		case SimilarityType::MinEqualityRotInv:maker.run<RotInvariantKernelDistance, GroupMinDistance>();
			break;
		case SimilarityType::Chain: maker.runChains();
			break;
#ifdef WITH_TORCH
		case SimilarityType::MSEOptim:
			for (size_t i = 0; i < numFrames; ++i)
			{
				if (kernel.size.x != kernel.size.y)
				{
					std::cout << "[Warning] Ignoring they y-size because mseoptim always uses a square kernel.\n";
				}
				std::vector<sf::Image> dstImages;
				dstImages.reserve(targetSheets.size());
				for (auto& sheet : targetSheets)
					dstImages.push_back(sheet.frames[i]);
				const unsigned numEpochs = kernel[0] <= 1.f ? 200 : static_cast<unsigned>(kernel[0]);
				auto map = nn::constructMapOptim(referenceSprites, dstImages, numThreads, kernel.size.x, numEpochs);
				
				if (minBorder)
					map = extendMap(map, originalSize, originalPosition);
				file << map;
			}
			break;
		case SimilarityType::EqualityMSEOptim:
			auto makeOptimSimilarity = [&](size_t frame)
			{
				std::vector<sf::Image> dstImages;
				dstImages.reserve(targetSheets.size());
				for (auto& sheet : targetSheets)
					dstImages.push_back(sheet.frames[frame]);
				auto map = nn::constructMapOptim(referenceSprites, dstImages, numThreads, 5, 128);
				return ScaleDistance(MapDistance(map), 0.5f);
			};
			maker.run<KernelDistance, GroupDistanceThreshold>(makeOptimSimilarity);
			break;
#endif
		default:
			std::cerr << "[Error] Invalid similarity type " << static_cast<int>(type) << ".\n";
			return 1;
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
				std::cout << "[Warning] Skipping the map "
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
			if (sheetMaps.empty())
			{
				std::cout << "[Warning] The file " << name << " does not contain a valid map.\n";
				transferMaps.pop_back();
			}
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
				{
					if (map.size != reference.getSize())
					{
						std::cerr << "[Error] Map with size (" 
							<< map.size.x << ", " << map.size.y 
							<< ") can not be applied to an image with size ("
							<< reference.getSize().x << ", " << reference.getSize().y << ")";
						std::abort();
					}
					else
						sheet.frames.emplace_back(applyMap(map, reference));
				}
				const std::filesystem::path mapName = targetNames[j];
				const std::string objectName = args::get(outputName);
				const std::string fileName = objectName + "_" + mapName.stem().string() + ".png";
				sheet.save(fileName);
			}
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "Computation took " << std::chrono::duration<double>(end - start).count() << "s." << std::endl;

	return 0;
}