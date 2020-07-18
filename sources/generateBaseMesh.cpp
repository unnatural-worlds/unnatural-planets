#include "generator.h"
#include "functions.h"
#include <cage-core/marchingCubes.h>

Holder<Polyhedron> generateBaseMesh(real size, uint32 resolution)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base mesh");
	OPTICK_EVENT();

#ifdef CAGE_DEBUG
	resolution /= 4;
#endif

	MarchingCubesCreateConfig cfg;
	cfg.box = aabb(vec3(size * -0.5), vec3(size * 0.5));
	cfg.resolutionX = cfg.resolutionY = cfg.resolutionZ = resolution;
	Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
	cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<&functionDensity>());
	Holder<Polyhedron> poly = cubes->makePolyhedron();
	{
		OPTICK_EVENT("discardInvalid");
		poly->discardInvalid(); // todo fix polyhedron clipping and remove this
	}
	{
		OPTICK_EVENT("discardDisconnected");
		poly->discardDisconnected();
	}
	return poly;
}





