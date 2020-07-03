#ifndef upmesh_h_sa5gt45t5vr1f
#define upmesh_h_sa5gt45t5vr1f

#include "common.h"
#include <cage-core/polyhedron.h>

#include <vector>

struct SplitResult
{
	std::vector<Holder<Polyhedron>> meshes;
};

SplitResult meshSplit(const Holder<Polyhedron> &mesh);
Holder<Polyhedron> meshSimplifyRegular(const Holder<Polyhedron> &mesh);
Holder<Polyhedron> meshSimplifyDynamic(const Holder<Polyhedron> &mesh);
Holder<Polyhedron> meshDiscardDisconnected(Holder<Polyhedron> &mesh);

void saveDebugMesh(const string &path, const Holder<Polyhedron> &mesh);
void saveRenderMesh(const string &path, const Holder<Polyhedron> &mesh);
void saveNavigationMesh(const string &path, const Holder<Polyhedron> &mesh, const std::vector<uint8> &terrainTypes);
void saveCollider(const string &path, const Holder<Polyhedron> &mesh);

real meshAverageEdgeLength(const Holder<Polyhedron> &mesh);

#endif
