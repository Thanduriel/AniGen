#include <random>
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>
#include <filesystem>

#include <args.hxx>
#include "spritesheet.hpp"
#include "map.hpp"
#include "pixelsimilarity.hpp"

using namespace math;

// compute the L1 difference between a and b
sf::Image imageDifference(const sf::Image& a, const sf::Image& b)
{
	sf::Image difImage;
	const sf::Vector2u size = a.getSize();
	difImage.create(size.x, size.y);

	for (unsigned y = 0; y < size.y; ++y)
	{
		for (unsigned x = 0; x < size.x; ++x)
		{
			sf::Color dif;
			const sf::Color col1 = a.getPixel(x, y);
			const sf::Color col2 = b.getPixel(x, y);

			dif.r = std::abs(static_cast<int>(col1.r) - static_cast<int>(col2.r));
			dif.g = std::abs(static_cast<int>(col1.g) - static_cast<int>(col2.g));
			dif.b = std::abs(static_cast<int>(col1.b) - static_cast<int>(col2.b));
			dif.a = 255;
			difImage.setPixel(x,y, dif);
		}
	}
	return difImage;
}

int main(int argc, char* argv[])
{
	args::ArgumentParser parser("Sprite animation generator.");
	
	args::Group commands(parser, "commands");
	args::Command applyMode(commands, "apply", "apply an existing map to a sprite");
	args::Command createMode(commands, "create", "create a map from reference sprites");
	
	args::Group arguments("arguments");
	args::HelpFlag help(arguments, "help", "display this help menu", { 'h', "help" });
	args::ValueFlag<int> framesInput(arguments, "num_input_frames", "number of frames in the input sprites", { 'n', "input_frames" }, 1);
	args::ValueFlag<int> framesTarget(arguments, "num_target_frames", "number of frames in the target sprites", { 'm', "target_frames" }, 1);
	args::ValueFlag<int> refFrame(arguments, "reference_frame", "which frame to use as reference in the input sprites; only necessary if input_frames > 0", { 'r', "ref_frame" }, 0);
	args::ValueFlag<unsigned> threads(arguments, "threads", "maximum number of threads to use", { 'j', "num_threads" }, std::thread::hardware_concurrency());
	args::ValueFlagList<std::string> inputs(arguments, "inputs", "the reference input sprites", { 'i', "inputs" });
	args::ValueFlagList<std::string> targets(arguments, "targets", "either the target sprites for the maps to (create) or the transfer maps to (apply)", { 't', "targets" });
	args::ValueFlag<std::string> outputName(arguments, "output", "name for the map to (create) or the object for (apply) without ending", { 'o', "output" });
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

	auto start = std::chrono::high_resolution_clock::now();

	const unsigned numThreads = args::get(threads);
	const bool useBlur = true || argc > 4 && !strcmp(argv[4], "blur");

	std::vector<sf::Image> referenceSprites;
	const auto inputNames = args::get(inputs);
	referenceSprites.reserve(inputNames.size());
	for (const auto& name : inputNames)
	{
		SpriteSheet sheet(name, args::get(framesInput));
		referenceSprites.emplace_back(sheet.frames[args::get(refFrame)]);
	}
	const auto targetNames = args::get(targets);

	if (createMode)
	{
		if (targetNames.size() != referenceSprites.size())
		{
			std::cerr << "[Error] The number of inputs (" << referenceSprites.size()
				<< ") and the number of targets (" << targetNames.size() << ") need to be equal.";
			return 1;
		}

	//	auto [_, animationName] = utils::splitName(targetNames[0]);
	/*	for (size_t i = 01; i < targetNames.size(); ++i)
		{
		//	auto [__, aniName] = utils::splitName(targetNames[0]);
			if (targetNames[i].find(animationName) == std::string::npos)
				std::cout << "[Warning] Encountered inconsistent animation names, could not find \""
				<< animationName << "\" in \""
				<< targetNames[i] << "\". All target sheets should be of the same animation";
		}*/

		const int numFrames = args::get(framesTarget);
		std::vector<SpriteSheet> targetSheets;
		targetSheets.reserve(targetNames.size());
		for (auto& name : targetNames)
			targetSheets.emplace_back(name, numFrames);

		const std::string mapName = args::get(outputName);
		std::ofstream file(mapName + ".txt");
		for (int i = 0; i < numFrames; ++i)
		{
			std::vector<KernelDistance> distances;
			for(size_t j = 0; j < targetSheets.size(); ++j)
				distances.emplace_back(referenceSprites[j], targetSheets[j].frames[i], sf::Vector2u(1, 1));
			const TransferMap map = constructMap(GroupDistance(std::move(distances)), numThreads);
			//constructMap(KernelDistance(reference.frames[0], reference.frames[i]), numThreads);

			file << map;
		}
	}
	else if (applyMode)
	{
		std::vector<std::vector<TransferMap>> transferMaps;
		transferMaps.reserve(targetNames.size());
		for (auto& name : targetNames)
		{
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

		for (size_t i = 0; i < referenceSprites.size(); ++i)
		{
			const sf::Image& reference = referenceSprites[i];
		//	const auto [baseName, _] = utils::splitName(inputNames[i]);

			for (size_t j = 0; j < transferMaps.size(); ++j)
			{
				auto& mapSheet = transferMaps[j];
				SpriteSheet sheet;
				for (TransferMap& map : mapSheet)
					sheet.frames.emplace_back(applyMap(map, reference));
				
				const std::filesystem::path mapName = targetNames[j];
			// const auto [_, animationName] = utils::splitName(targetNames[j]);
			//	sheet.save(std::string(baseName) + "_" + std::string(animationName) + ".png");
				const std::string objectName = args::get(outputName);
				sheet.save(objectName + "_" + mapName.stem().string() + ".png");
			}
		}
	}

/*	SpriteSheet reference(refName, numFrames);
	SpriteSheet validation(targetName, numFrames);
	const auto minRect = computeMinRect({ &reference,&validation }, 3u);
	reference.crop(minRect);
	validation.crop(minRect);

	SpriteSheet output;
	SpriteSheet dif;
	output.frames.emplace_back(validation.frames[0]);
	dif.frames.emplace_back(imageDifference(validation.frames[0], output.frames[0]));
	for (int i = 1; i < 8; ++i)
	{
		const TransferMap map = useBlur ? constructMapAvg(reference.frames[0], reference.frames[i], numThreads)
			: constructMap(reference.frames[0], reference.frames[i], numThreads);
		colorMap(map, reference.frames[0]).saveToFile("distance" + std::to_string(i) + ".png");
	//	distanceMap(map).saveToFile("distance" + std::to_string(i) + ".png");
		output.frames.emplace_back(applyMap(map, validation.frames[i]));
		dif.frames.emplace_back(imageDifference(validation.frames[i], output.frames.back()));
	}
/*	TransferMap transferMap(frameSize);
	for (unsigned y = 0; y < frameSize.y; ++y)
	{
		const unsigned idy = y * frameSize.x;
		for (unsigned x = 0; x < frameSize.x; ++x)
		{
			transferMap.elements[x + idy] = sf::Vector2u(x, frameSize.y - y - 1);
		}
	}
	dif.save("dif.png");
	output.save("output.png");*/

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "Computation took " << std::chrono::duration<double>(end - start).count() << "s" << std::endl;

	return 0;
}