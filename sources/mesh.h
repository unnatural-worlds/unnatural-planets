#ifndef mesh_h_eszh489
#define mesh_h_eszh489

#include <cage-core/core.h>

#include <vector>

using namespace cage;

struct Tile;

Holder<Polyhedron> meshGenerateBaseLand();
Holder<Polyhedron> meshGenerateBaseWater();
Holder<Polyhedron> meshGenerateBaseNavigation();
std::vector<Holder<Polyhedron>> meshSplit(const Holder<Polyhedron> &mesh);
void meshSimplifyNavmesh(Holder<Polyhedron> &mesh);
void meshSimplifyCollider(Holder<Polyhedron> &mesh);
void meshSimplifyRender(Holder<Polyhedron> &mesh);
uint32 meshUnwrap(const Holder<Polyhedron> &mesh);

void meshSaveDebug(const string &path, const Holder<Polyhedron> &mesh);
void meshSaveRender(const string &path, const Holder<Polyhedron> &mesh, bool transparency);
void meshSaveNavigation(const string &path, const Holder<Polyhedron> &mesh, const std::vector<Tile> &tiles);
void meshSaveCollider(const string &path, const Holder<Polyhedron> &mesh);

#endif
