#ifndef generator_h_4hgtd6
#define generator_h_4hgtd6

#include <cage-core/math.h>

#include <vector>

using namespace cage;

struct Tile;

Holder<Polyhedron> generateBaseMesh();
void generateTileProperties(const Holder<Polyhedron> &navMesh, std::vector<Tile> &tiles, const string &statsLogPath);
void generateDoodads(const Holder<Polyhedron> &navMesh, const std::vector<Tile> &tiles, std::vector<string> &assetPackages, const string &doodadsPath, const string &statsLogPath);
void generateTextures(const Holder<Polyhedron> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);
void generateEntry();
string generateName();

#endif
