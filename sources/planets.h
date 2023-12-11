#ifndef planets_h_fgh4uj85g
#define planets_h_fgh4uj85g

#include <vector>

#include <cage-core/math.h>

namespace cage
{
	class Mesh;
	class Image;
}

namespace unnatural
{
	using namespace cage;

	enum class TerrainBiomeEnum : uint8
	{
		// clang-format off
		// inspired by Whittaker diagram
		//                        // temperature // precipitation // coverage // alternate names
		//                        //    (°C)     //     (cm)      //   (%)    //
		Bare,                     // -15 .. -5   //    0 ..  10   //          //
		Tundra,                   // -15 .. -5   //   10 ..  30   //    11    //
		Taiga,                    //  -5 ..  5   //   30 .. 440   //          // boreal forest
		Shrubland,                //   5 .. 20   //    0 ..  30   //     3    //
		Grassland,                //   5 .. 20   //   30 .. 100   //    13    //
		TemperateSeasonalForest,  //   5 .. 20   //  100 .. 200   //     4    // temperate deciduous forest
		TemperateRainForest,      //   5 .. 20   //  200 .. 300   //     4    //
		Desert,                   //  20 .. 30   //    0 ..  40   //    19    // subtropical desert
		Savanna,                  //  20 .. 30   //   40 .. 130   //    10    // tropical grassland
		TropicalSeasonalForest,   //  20 .. 30   //  130 .. 230   //     6    //
		TropicalRainForest,       //  20 .. 30   //  230 .. 440   //     6    //
		Water,
		_Total
		// clang-format on
	};

	enum class TerrainTypeEnum : uint8
	{
		Road = 0,
		Flat = 1,
		Rough = 2,
		Cliffs = 3,
		ShallowWater = 4,
		DeepWater = 5,
		_Total
	};

	enum class MeshPurposeEnum : uint8
	{
		Undefined = 0,
		Land,
		Water,
		Navigation,
	};

	Stringizer &operator+(Stringizer &str, const TerrainBiomeEnum &other);
	Stringizer &operator+(Stringizer &str, const TerrainTypeEnum &other);

	struct DoodadDefinition;

	struct Tile
	{
		std::vector<uint32> neighbors;
		Vec3 position;
		Vec3 normal;
		Vec3 albedo;
		Real roughness;
		Real metallic;
		Real height;
		Real elevation;
		Rads slope;
		Real temperature;
		Real precipitation;
		Real opacity = 1;
		Real flatRadius;
		const DoodadDefinition *doodad = nullptr;
		TerrainBiomeEnum biome = TerrainBiomeEnum::_Total;
		TerrainTypeEnum type = TerrainTypeEnum::_Total;
		MeshPurposeEnum meshPurpose = MeshPurposeEnum::Undefined;
		bool buildable = false;
	};

	struct DoodadDefinition
	{
		String name;
		String package;
		String proto;
		uint32 instances = 0; // how many have actually been placed

		// generating
		Real priority; // determines which doodads should be placed first
		uint32 maxCount = m;
		Real chance = 1; // chance to place a doodad on a valid tile

		// requirements
		Vec2 temperature = Vec2(-Real::Infinity(), Real::Infinity());
		Vec2 precipitation = Vec2(0, Real::Infinity());
		Vec2 elevation = Vec2(-Real::Infinity(), Real::Infinity());
		Vec2 slope = Vec2(0, Real::Infinity()); // degs in file, rads in memory
		Vec2 startsDistance = Vec2(0, Real::Infinity());
		Vec2i startsCount = Vec2i(0, m);
		Real radius = 5; // distance in which to prevent overlapping with other doodads
		bool buildable = false;

		Real previewHeight = Real::Nan();
	};

	extern std::vector<Tile> tiles;
	extern std::vector<DoodadDefinition> doodadsDefinitions;
	extern std::vector<String> assetPackages;
	extern std::vector<uint32> startingPositions;

	extern const String baseDirectory;
	extern const String assetsDirectory;
	extern const String debugDirectory;
}

#endif
