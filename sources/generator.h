#ifndef generator_h_4hgtd6
#define generator_h_4hgtd6

#include <cage-core/math.h>
#include <cage-core/polyhedron.h>

#include <vector>
#include <optick.h>

using namespace cage;

enum class BiomeEnum : uint8
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
	_Ocean,
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

stringizer &operator + (stringizer &str, const BiomeEnum &other);
stringizer &operator + (stringizer &str, const TerrainTypeEnum &other);

void functionsConfigure(const Holder<Ini> &cmd);
void meshConfigure(const Holder<Ini> &cmd);

real functionDensity(const vec3 &pos);
void functionTileProperties(const vec3 &pos, const vec3 &normal, BiomeEnum &biome, TerrainTypeEnum &terrainType, real &elevation, real &temperature, real &precipitation);
void functionMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height);

Holder<Polyhedron> generateBaseMesh(real size, uint32 resolution);
void generateTileProperties(const Holder<Polyhedron> &navMesh, std::vector<TerrainTypeEnum> &tileTypes, std::vector<BiomeEnum> &tileBiomes, std::vector<real> &tileElevations, std::vector<real> &tileTemperatures, std::vector<real> &tilePrecipitations, const string &statsLogPath);
void generateMaterials(const Holder<Polyhedron> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);
void generateDoodads(const Holder<Polyhedron> &navMesh, const std::vector<TerrainTypeEnum> &tileTypes, const std::vector<BiomeEnum> &tileBiomes, const std::vector<real> &tileElevations, const std::vector<real> &tileTemperatures, const std::vector<real> &tilePrecipitations, const string &doodadsPath);

std::vector<Holder<Polyhedron>> meshSplit(const Holder<Polyhedron> &mesh);
void meshSimplifyNavmesh(Holder<Polyhedron> &mesh);
void meshSimplifyCollider(Holder<Polyhedron> &mesh);
void meshSimplifyRender(Holder<Polyhedron> &mesh);
uint32 meshUnwrap(const Holder<Polyhedron> &mesh);

void saveDebugMesh(const string &path, const Holder<Polyhedron> &mesh);
void saveRenderMesh(const string &path, const Holder<Polyhedron> &mesh);
void saveNavigationMesh(const string &path, const Holder<Polyhedron> &mesh, const std::vector<uint8> &terrainTypes);
void saveCollider(const string &path, const Holder<Polyhedron> &mesh);

void generateEntry();

#endif
