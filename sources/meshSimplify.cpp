#include "mesh.h"

#include <pmp/SurfaceMesh.h>
#include <pmp/algorithms/SurfaceRemeshing.h>

Holder<UPMesh> meshSimplifyRegular(const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing mesh");
	OPTICK_EVENT();
	// todo
	return detail::systemArena().createHolder<UPMesh>(*mesh);
}

Holder<UPMesh> meshSimplifyDynamic(const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying mesh");
	OPTICK_EVENT();
	// todo
	return detail::systemArena().createHolder<UPMesh>(*mesh);
}

