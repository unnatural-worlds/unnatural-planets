#include <cage-core/polyhedron.h>
#include <cage-core/marchingCubes.h>

#include "terrain.h"
#include "generator.h"

Holder<Polyhedron> generateBaseMesh()
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base mesh");

	constexpr real size = 2500;
#ifdef CAGE_DEBUG
	constexpr uint32 resolution = 200 / 3;
#else
	constexpr uint32 resolution = 200;
#endif

	MarchingCubesCreateConfig cfg;
	cfg.box = aabb(vec3(size * -0.5), vec3(size * 0.5));
	cfg.resolution = ivec3(resolution);
	Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
	cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<&terrainShape>());
	Holder<Polyhedron> poly = cubes->makePolyhedron();
	//poly->exportObjFile({}, "debug/1.obj");
	polyhedronDiscardDisconnected(+poly);
	//poly->exportObjFile({}, "debug/2.obj");
	if (poly->indicesCount() == 0)
		CAGE_THROW_ERROR(Exception, "generated empty base mesh");
	return poly;
}
