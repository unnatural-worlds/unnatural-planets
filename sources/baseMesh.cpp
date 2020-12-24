#include <cage-core/polyhedron.h>
#include <cage-core/marchingCubes.h>

#include "terrain.h"
#include "generator.h"

Holder<Polyhedron> generateBaseMesh(real size, uint32 resolution)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base mesh");

#ifdef CAGE_DEBUG
	resolution /= 3;
#endif

	MarchingCubesCreateConfig cfg;
	cfg.box = aabb(vec3(size * -0.5), vec3(size * 0.5));
	cfg.resolution = ivec3(resolution);
	Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
	cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<&terrainDensity>());
	Holder<Polyhedron> poly = cubes->makePolyhedron();
	//poly->exportObjFile({}, "debug/1.obj");
	polyhedronDiscardDisconnected(+poly);
	//poly->exportObjFile({}, "debug/2.obj");
	if (poly->indicesCount() == 0)
		CAGE_THROW_ERROR(Exception, "generated empty base mesh");
	return poly;
}
