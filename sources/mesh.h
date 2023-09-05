#ifndef mesh_h_eszh489
#define mesh_h_eszh489

#include <vector>

#include "planets.h"

namespace unnatural
{
	Holder<Mesh> meshGenerateBaseLand();
	Holder<Mesh> meshGenerateBaseWater();
	Holder<Mesh> meshGenerateBaseNavigation();
	Holder<PointerRange<Holder<Mesh>>> meshSplit(const Holder<Mesh> &mesh);
	void meshSimplifyNavmesh(Holder<Mesh> &mesh);
	void meshSimplifyCollider(Holder<Mesh> &mesh);
	void meshSimplifyRender(Holder<Mesh> &mesh);
	uint32 meshUnwrap(const Holder<Mesh> &mesh);

	void meshSaveDebug(const String &path, const Holder<Mesh> &mesh);
	void meshSaveRender(const String &path, const Holder<Mesh> &mesh, bool transparency);
	void meshSaveNavigation(const String &path, const Holder<Mesh> &mesh, const std::vector<Tile> &tiles);
	void meshSaveCollider(const String &path, const Holder<Mesh> &mesh);
}

#endif
