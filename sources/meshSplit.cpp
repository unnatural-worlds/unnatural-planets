#include "mesh.h"

SplitResult meshSplit(const Holder<Polyhedron> &mesh)
{
	SplitResult res;
	// todo
	res.meshes.push_back(mesh->copy());
	return res;
}
