#ifndef terrain_h_89w7ertuj
#define terrain_h_89w7ertuj

#include <cage-core/math.h>

using namespace cage;

enum class TerrainBiomeEnum : uint8
{
	// inspired by Whittaker diagram
	//                        // temperature // precipitation // coverage //
	//                        //    (°C)     //     (cm)      //   (%)    //
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
	Water,
	_Total
};

enum class TerrainTypeEnum : uint8
{
	Road = 0,
	Fast = 1,
	Slow = 2,
	SteepSlope = 3,
	ShallowWater = 4,
	DeepWater = 5,
	_Total
};

stringizer &operator + (stringizer &str, const TerrainBiomeEnum &other);
stringizer &operator + (stringizer &str, const TerrainTypeEnum &other);

struct Tile
{
	vec3 position;
	vec3 normal;
	vec3 albedo;
	real roughness;
	real metallic;
	real height; // bump map value
	real elevation; // (meters) above sea
	rads slope;
	real temperature; // °C average annual
	real precipitation; // cm total annual
	TerrainBiomeEnum biome;
	TerrainTypeEnum type;
};

void terrainTile(Tile &tile); // input: position and normal, output: everything else
real terrainElevation(const vec3 &pos);
real terrainDensity(const vec3 &pos);
void terrainPreseed();
void terrainApplyConfig();

#endif
