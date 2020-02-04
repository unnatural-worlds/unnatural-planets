#ifndef upmesh_h_sa5gt45t5vr1f
#define upmesh_h_sa5gt45t5vr1f

#include "common.h"

#include <vector>

struct UPMesh
{
	std::vector<vec3> positions;
	std::vector<vec3> normals;
	std::vector<vec2> uvs;
	std::vector<uint32> indices;
};

struct UnwrapResult
{
	std::vector<Holder<UPMesh>> meshes;
	uint32 textureWidth = 0;
	uint32 textureHeight = 0;
};

Holder<UPMesh> newUPMesh();

UnwrapResult meshUnwrap(const Holder<UPMesh> &mesh);
Holder<UPMesh> meshSimplifyRegular(const Holder<UPMesh> &mesh);
Holder<UPMesh> meshSimplifyDynamic(const Holder<UPMesh> &mesh);

void saveRenderMesh(const string &path, const Holder<UPMesh> &mesh);
void saveNavigationMesh(const string &path, const Holder<UPMesh> &mesh, const std::vector<uint8> &terrainTypes);
void saveCollider(const string &path, const Holder<UPMesh> &mesh);

#endif
