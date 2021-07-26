#include <random>
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>

#include <args.hxx>
#include "spritesheet.hpp"
#include "map.hpp"
#include "tests/tests.hpp"

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
	args::ValueFlag<int> frames(arguments, "num_frames", "number of frames in the sprite", { 'n', "num_frames" }, 8);
	args::ValueFlag<unsigned> threads(arguments, "threads", "maximum number of threads to use", { 't', "num_threads" }, std::thread::hardware_concurrency());
	args::ValueFlagList<std::string> inputs(arguments, "inputs", "the input sprites", { 'i', "inputs" });
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

//	const std::string refName = "../Sprites/HumanMale_ApronChest.png";
//	const std::string targetName = "../Sprites/HumanMale_FamilyChest_Medium.png";
	const int numFrames = args::get(frames);
	const unsigned numThreads = args::get(threads);
	const bool useBlur = true || argc > 4 && !strcmp(argv[4], "blur");

	Tests tests; 
	tests.run();
	if (createMode)
	{
		std::vector<SpriteSheet> spriteSheets;
		auto names = args::get(inputs);
		spriteSheets.reserve(names.size());
		for (const auto& name : names)
			spriteSheets.emplace_back(name, numFrames);

		auto reference = spriteSheets.front();
		for (int i = 1; i < numFrames; ++i)
		{
			const TransferMap map = useBlur ? constructMap(BlurDistance(reference.frames[0], reference.frames[i]), numThreads)
				: constructMap(KernelDistance(reference.frames[0], reference.frames[i]), numThreads);

			std::ofstream file("transferMap" + std::to_string(i) + ".txt");
			file << map;
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