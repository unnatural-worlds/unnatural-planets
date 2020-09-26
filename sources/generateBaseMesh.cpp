#include <cage-core/marchingCubes.h>

#include "generator.h"

Holder<Polyhedron> generateBaseMesh(real size, uint32 resolution)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base mesh");
	OPTICK_EVENT();

#ifdef CAGE_DEBUG
	resolution /= 4;
#endif

	MarchingCubesCreateConfig cfg;
	cfg.box = aabb(vec3(size * -0.5), vec3(size * 0.5));
	cfg.resolution = ivec3(resolution);
	Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
	cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<&functionDensity>());
	Holder<Polyhedron> poly = cubes->makePolyhedron();
	//poly->exportObjFile({}, "debug/1.obj");
	{
		OPTICK_EVENT("discardDisconnected");
		polyhedronDiscardDisconnected(+poly);
	}
	//poly->exportObjFile({}, "debug/2.obj");
	return poly;
}





