#include <map.hpp>
#include <math/vectorext.hpp>

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

void main()
{
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

	std::cout << "\nSuccessfully finished tests " << testsRun - testsFailed << "/" << testsRun << "\n";
}