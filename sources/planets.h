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
		uint32 instances = 0;

		String package;
		String proto;

		// requirements
		Vec2 temperature = Vec2(-Real::Infinity(), Real::Infinity());
		Vec2 precipitation = Vec2(-Real::Infinity(), Real::Infinity());
		Real probability;
		uint32 starting = 0; // number of instances of the doodad that must be near every starting position
		bool ocean = false;
		bool slope = false;
		bool buildable = false;
	};

	extern std::vector<Tile> tiles;
	extern std::vector<DoodadDefinition> doodadsDefinitions;
	extern std::vector<String> assetPackages;

	extern const String baseDirectory;
	extern const String assetsDirectory;
	extern const String debugDirectory;
}

#endif
