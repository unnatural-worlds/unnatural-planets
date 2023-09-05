#ifndef terrain_h_89w7ertuj
#define terrain_h_89w7ertuj

#include "planets.h"

namespace unnatural
{
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
		SteepSlope = 3,
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
		const Doodad *doodad = nullptr;
		TerrainBiomeEnum biome = TerrainBiomeEnum::_Total;
		TerrainTypeEnum type = TerrainTypeEnum::_Total;
		MeshPurposeEnum meshPurpose = MeshPurposeEnum::Undefined;
		bool buildable = false;
	};

	void terrainTile(Tile &tile);
}

#endif
