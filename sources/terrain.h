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
	Taiga,                    
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

Stringizer &operator + (Stringizer &str, const TerrainBiomeEnum &other);
Stringizer &operator + (Stringizer &str, const TerrainTypeEnum &other);

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
	Real flatRadius = 0;
	TerrainBiomeEnum biome;
	TerrainTypeEnum type;
	bool buildable = false;
};

Real terrainSdfElevation(const Vec3 &pos);
Real terrainSdfElevationRaw(const Vec3 &pos);
Real terrainSdfLand(const Vec3 &pos);
Real terrainSdfWater(const Vec3 &pos);
Real terrainSdfNavigation(const Vec3 &pos);
void terrainTileLand(Tile &tile);
void terrainTileWater(Tile &tile);
void terrainTileNavigation(Tile &tile);
void terrainPreseed();
void terrainApplyConfig();

#endif
