#ifndef generator_h_4hgtd6
#define generator_h_4hgtd6

#include <cage-core/math.h>

#include <vector>

using namespace cage;

struct Tile;

void generateTileProperties(const Holder<Mesh> &navMesh, std::vector<Tile> &tiles, const string &statsLogPath);
void generateDoodads(const Holder<Mesh> &navMesh, const std::vector<Tile> &tiles, std::vector<string> &assetPackages, const string &doodadsPath, const string &statsLogPath);
void generateTexturesLand(const Holder<Mesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);
void generateTexturesWater(const Holder<Mesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);
void generateEntry();
string generateName();

#endif
