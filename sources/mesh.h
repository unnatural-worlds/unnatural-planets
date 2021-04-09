#ifndef mesh_h_eszh489
#define mesh_h_eszh489

#include <cage-core/core.h>

#include <vector>

using namespace cage;

struct Tile;

Holder<Mesh> meshGenerateBaseLand();
Holder<Mesh> meshGenerateBaseWater();
Holder<Mesh> meshGenerateBaseNavigation();
std::vector<Holder<Mesh>> meshSplit(const Holder<Mesh> &mesh);
void meshSimplifyNavmesh(Holder<Mesh> &mesh);
void meshSimplifyCollider(Holder<Mesh> &mesh);
void meshSimplifyRender(Holder<Mesh> &mesh);
uint32 meshUnwrap(const Holder<Mesh> &mesh);

void meshSaveDebug(const string &path, const Holder<Mesh> &mesh);
void meshSaveRender(const string &path, const Holder<Mesh> &mesh, bool transparency);
void meshSaveNavigation(const string &path, const Holder<Mesh> &mesh, const std::vector<Tile> &tiles);
void meshSaveCollider(const string &path, const Holder<Mesh> &mesh);

#endif
