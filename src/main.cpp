#include <random>
#include <chrono>
#include <iostream>

#include "spritesheet.hpp"
#include "map.hpp"

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
	auto start = std::chrono::high_resolution_clock::now();

	std::string refName = "../Sprites/HumanMale_ApronChest.png";
	std::string targetName = "../Sprites/HumanMale_FamilyChest_Medium.png";
	int numFrames = 8;
	if (argc > 2)
	{
		refName = argv[1];
		targetName = argv[2];
	}
	if (argc > 3)
		numFrames = std::stoi(argv[3]);

	SpriteSheet reference(refName, numFrames);
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
		const TransferMap map = constructMap(reference.frames[0], reference.frames[i], 4);
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
	}*/
	dif.save("dif.png");
	output.save("output.png");

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "Computation took " << std::chrono::duration<double>(end - start).count() << "s" << std::endl;

	return 0;
}