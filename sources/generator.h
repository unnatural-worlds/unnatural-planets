#ifndef generator_h_4hgtd6
#define generator_h_4hgtd6

#include "mesh.h"

Holder<Polyhedron> generateBaseMesh(real size, uint32 resolution);
std::vector<uint8> generateTileProperties(const Holder<Polyhedron> &navMesh, const string &statsLogPath);
void generateMaterials(const Holder<Polyhedron> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);

void generateEntry();

void functionsConfigure(const Holder<Ini> &cmd);

#endif
