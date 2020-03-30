#ifndef functions_h_4hges9
#define functions_h_4hges9

#include "common.h"

enum class BiomeEnum
{
	// inspired by Whittaker diagram
	// temperature // precipitation // coverage //
	//    (°C)     //     (cm)      //   (%)    //
	Bare,                     // -15 .. -5   //    0 ..  10   //          //
	Tundra,                   // -15 .. -5   //   10 ..  30   //    11    //
	Taiga,                    //  -5 ..  5   //   30 .. 150   //    17    // (BorealForest)
	Shrubland,                //   5 .. 20   //    0 ..  30   //     3    //
	Grassland,                //   5 .. 20   //   30 .. 100   //    13    //
	TemperateSeasonalForest,  //   5 .. 20   //  100 .. 200   //     4    // (TemperateDeciduousForest)
	TemperateRainForest,      //   5 .. 20   //  200 .. 300   //     4    //
	Desert,                   //  20 .. 30   //    0 ..  40   //    19    // (SubtropicalDesert)
	Savanna,                  //  20 .. 30   //   40 .. 130   //    10    // (TropicalGrassland)
	TropicalSeasonalForest,   //  20 .. 30   //  130 .. 230   //     6    //
	TropicalRainForest,       //  20 .. 30   //  230 .. 440   //     6    //
	Ocean = 250,
};

enum class TerrainTypeEnum
{
	Road = 0,
	Slow = 2,
	Fast = 5,
	SteepSlope = 4,
	ShallowWater = 7,
	DeepWater = 8,
};

stringizer &operator + (stringizer &str, const BiomeEnum &other);
stringizer &operator + (stringizer &str, const TerrainTypeEnum &other);

real functionDensity(const vec3 &pos);
void functionTileProperties(const vec3 &pos, const vec3 &normal, BiomeEnum &biome, TerrainTypeEnum &terrainType);
void functionMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height);

#endif
