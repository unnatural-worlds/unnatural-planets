#include "mesh.h"

namespace
{
#ifdef CAGE_DEBUG
	const uint32 iterations = 1;
	const float targetScale = 3;
#else
	const uint32 iterations = 10;
	const float targetScale = 1;
#endif // CAGE_DEBUG
}

Holder<Polyhedron> meshSimplifyRegular(const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing mesh");
	OPTICK_EVENT();

	PolyhedronRegularizationConfig cfg;
	cfg.iterations = iterations;
	cfg.targetEdgeLength = targetScale;
	Holder<Polyhedron> m = mesh->copy();
	m->regularize(cfg);
	return m;
}

Holder<Polyhedron> meshSimplifyDynamic(const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying mesh");
	OPTICK_EVENT();

	PolyhedronSimplificationConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.5 * targetScale;
	cfg.maxEdgeLength = 10 * targetScale;
	cfg.approximateError = 0.03 * targetScale;
	Holder<Polyhedron> m = mesh->copy();
	m->simplify(cfg);

	if (m->indicesCount() < mesh->indicesCount())
		return m;
	CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified mesh has more triangles than the original");
	return mesh->copy(); // make a fresh copy
}

Holder<Polyhedron> meshDiscardDisconnected(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "detecting disconnected parts");
	OPTICK_EVENT();

	Holder<Polyhedron> m = mesh->copy();
	m->discardDisconnected();
	return m;
}
