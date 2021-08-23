#include <map.hpp>
#include <math/vectorext.hpp>
#include <math/convolution.hpp>

#include <numeric>
#include <iostream>
#include <random>
#include <fstream>

# define EXPECT(cond,description)										\
do {																	\
	++testsRun;															\
	if (!(cond)) {														\
	  std::cerr << "FAILED "											\
				<< description << std::endl <<"       " << #cond		\
				<< std::endl											\
				<< "       " << __FILE__ << ':' << __LINE__				\
				<< std::endl;											\
	  ++testsFailed;													\
	}																	\
} while (false)

int testsRun = 0;
int testsFailed = 0;

int main()
{
	// basic serialization
	std::default_random_engine rng(0xAF31EB77);
	std::uniform_int_distribution<unsigned> dist(0u, 255u);

	TransferMap map(sf::Vector2u(64u, 64u));
	for (auto& el : map.elements)
		el = sf::Vector2u(dist(rng), dist(rng));

	{
		std::ofstream outFile("test.txt");
		outFile << map;
	}
	{
		std::ifstream inFile("test.txt");
		TransferMap storedMap;
		inFile >> storedMap;
		EXPECT(map.size.x == storedMap.size.x && map.size.y == storedMap.size.y, "loaded map has correct dimensions");
		EXPECT(std::memcmp(map.elements.data(), storedMap.elements.data(), map.elements.size() * sizeof(decltype(map.elements.front()))) == 0, "loaded map is correct");
	}
	{
		std::ofstream outFile("test.dat", std::ios::binary);
		map.save(outFile);
	}
	{
		std::ifstream inFile("test.dat", std::ios::binary);
		const TransferMap storedMap(inFile);
		EXPECT(map.size.x == storedMap.size.x && map.size.y == storedMap.size.y, "binary loaded map has correct dimensions");
		EXPECT(std::memcmp(map.elements.data(), storedMap.elements.data(), map.elements.size() * sizeof(decltype(map.elements.front()))) == 0, "binary loaded map is correct");
	}

	// convolution
	{
		using namespace math;
		sf::Image image;
		sf::Vector2u size(8, 16);
		image.create(size.x, size.y, sf::Color(255,0,0));
		for (unsigned x = 0; x < size.x; ++x)
			image.setPixel(x, x, sf::Color(0, 4, 0));

		const Matrix<int> scalarKernel(sf::Vector2u(1,1), 2);

		auto sample = [](int s, const sf::Color& color)
		{
			return s * sf::Vector3i(color.r,color.g,color.b);
		};

		auto sum = [](const Matrix<sf::Vector3i>& result)
		{
			return std::accumulate(result.begin(), result.end(), sf::Vector3i{});
		};

		auto scalarConv = applyConvolution(image, scalarKernel, sample, sum);
		EXPECT(scalarConv.size == image.getSize(), "scalar convolution preserves the size");
		int wrongResults = 0;
		for (unsigned y = 0; y < size.y; ++y)
			for (unsigned x = 0; x < size.x; ++x)
			{
				if (x == y)
				{
					if (scalarConv(x, y) != sf::Vector3i(0, 8, 0))
						++wrongResults;
				}
				else if (scalarConv(x, y) != sf::Vector3i(510, 0, 0))
					++wrongResults;
			}
		EXPECT(wrongResults == 0, "scalar convolution is applied correctly");

		Matrix<int> largeKernel(sf::Vector2u(3, 5), 0);
	//	for (int i = 0; i < 3; ++i)
		largeKernel(1, 4) = 1;

		auto largeConv = applyConvolution(image, largeKernel, sample, sum);
		EXPECT(largeConv.size == image.getSize(), "padded convolution preserves the size");
		wrongResults = 0;
		for (unsigned y = 2; y < size.y; ++y)
			for (unsigned x = 0; x < size.x; ++x)
			{
				const sf::Color color = image.getPixel(x, y);
				const sf::Vector3i colorVec(color.r, color.g, color.b);
				if (largeConv(x, y-2) != colorVec)
					++wrongResults;
			}
		for (unsigned y = size.y - 2; y < size.y; ++y)
			for (unsigned x = 0; x < size.x; ++x)
				if (largeConv(x, y) != sf::Vector3i{})
					++wrongResults;
		EXPECT(wrongResults == 0, "padded convolution is applied correctly");
	}

	std::cout << "\nSuccessfully finished tests " << testsRun - testsFailed << "/" << testsRun << "\n";

	return testsFailed;
}