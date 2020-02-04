#ifndef generator_h_4hgtd6
#define generator_h_4hgtd6

#include "mesh.h"
#include "texture.h"

Holder<UPMesh> generateBaseMesh(real size, uint32 resolution);
std::vector<uint8> generateTileProperties(const Holder<UPMesh> &navMesh);
void generateMaterials(const Holder<UPMesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);

void generateEntry();

#endif
