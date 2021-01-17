#include <cage-core/geometry.h>
#include <cage-core/config.h>
#include <unnatural-navmesh/navmesh.h>

#include "mesh.h"

#include <initializer_list>

namespace
{
#ifdef CAGE_DEBUG
	constexpr uint32 iterations = 1;
	constexpr float tileSize = 30;
#else
	constexpr uint32 iterations = 10;
	constexpr float tileSize = 10;
#endif // CAGE_DEBUG

	ConfigBool configNavmeshOptimize("unnatural-planets/navmesh/optimize");

	real meshSurfaceArea(const Polyhedron *mesh)
	{
		const auto inds = mesh->indices();
		const auto poss = mesh->positions();
		const uint32 cnt = numeric_cast<uint32>(inds.size() / 3);
		real result = 0;
		for (uint32 ti = 0; ti < cnt; ti++)
		{
			const triangle t = triangle(poss[inds[ti * 3 + 0]], poss[inds[ti * 3 + 1]], poss[inds[ti * 3 + 2]]);
			result += t.area();
		}
		return result;
	}

	uint32 boxLongestAxis(const aabb &box)
	{
		const vec3 mySizes = box.size();
		const vec3 a = abs(dominantAxis(mySizes));
		if (a[0] == 1)
			return 0;
		if (a[1] == 1)
			return 1;
		return 2;
	}

	aabb clippingBox(const aabb &box, uint32 axis, real pos, bool second = false)
	{
		const vec3 c = box.center();
		const vec3 hs = box.size() * 0.6; // slightly larger box to avoid clipping due to floating point imprecisions
		aabb r = aabb(c - hs, c + hs);
		if (second)
			r.a[axis] = pos;
		else
			r.b[axis] = pos;
		return r;
	}
}

void meshSimplifyNavmesh(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing navigation mesh");

	if (configNavmeshOptimize)
	{
		unnatural::NavmeshOptimizationConfig cfg;
#ifdef CAGE_DEBUG
		cfg.iterations = 1;
#endif
		cfg.tileSize = tileSize;
		mesh = unnatural::navmeshOptimize(templates::move(mesh), cfg);
	}
	else
	{
		PolyhedronRegularizationConfig cfg;
		cfg.iterations = iterations;
		cfg.targetEdgeLength = tileSize;
		polyhedronRegularize(+mesh, cfg);
	}
}

void meshSimplifyCollider(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying collider mesh");

	PolyhedronSimplificationConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.5 * tileSize;
	cfg.maxEdgeLength = 10 * tileSize;
	cfg.approximateError = 0.03 * tileSize;
	Holder<Polyhedron> m = mesh->copy();
	polyhedronSimplify(+m, cfg);

	if (m->indicesCount() < mesh->indicesCount())
		mesh = templates::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified collider mesh has more triangles than the original");
}

void meshSimplifyRender(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying render mesh");

	PolyhedronSimplificationConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.5 * tileSize;
	cfg.maxEdgeLength = 10 * tileSize;
	cfg.approximateError = 0.03 * tileSize;
	Holder<Polyhedron> m = mesh->copy();
	polyhedronSimplify(+m, cfg);

	if (m->indicesCount() < mesh->indicesCount())
		mesh = templates::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified render mesh has more triangles than the original");
}

std::vector<Holder<Polyhedron>> meshSplit(const Holder<Polyhedron> &mesh)
{
	const real myArea = meshSurfaceArea(+mesh);
	std::vector<Holder<Polyhedron>> result;
	if (myArea > 250000)
	{
		const aabb myBox = mesh->boundingBox();
		const uint32 a = boxLongestAxis(myBox);
		real bestSplitPosition = 0.5;
		real bestSplitScore = real::Infinity();
		for (real position : { 0.3, 0.4, 0.45, 0.5, 0.55, 0.6, 0.7 })
		{
			Holder<Polyhedron> p = mesh->copy();
			polyhedronClip(+p, clippingBox(myBox, a, interpolate(myBox.a[a], myBox.b[a], position)));
			const real area = meshSurfaceArea(p.get());
			const real score = abs(0.5 - area / myArea);
			if (score < bestSplitScore)
			{
				bestSplitScore = score;
				bestSplitPosition = position;
			}
		}
		const real split = interpolate(myBox.a[a], myBox.b[a], bestSplitPosition);
		Holder<Polyhedron> m1 = mesh->copy();
		Holder<Polyhedron> m2 = mesh->copy();
		polyhedronClip(+m1, clippingBox(myBox, a, split));
		polyhedronClip(+m2, clippingBox(myBox, a, split, true));
		result = meshSplit(m1);
		std::vector<Holder<Polyhedron>> r2 = meshSplit(m2);
		for (auto &it : r2)
			result.push_back(templates::move(it));
	}
	else
	{
		// no more splitting is required
		result.push_back(mesh->copy());
	}
	return result;
}

uint32 meshUnwrap(const Holder<Polyhedron> &mesh)
{
	PolyhedronUnwrapConfig cfg;
	cfg.maxChartIterations = 10;
	cfg.maxChartBoundaryLength = 500;
	cfg.chartRoundness = 0.3;
#ifdef CAGE_DEBUG
	cfg.texelsPerUnit = 0.3;
#else
	cfg.texelsPerUnit = 2.5;
#endif // CAGE_DEBUG
	cfg.padding = 6;
	return polyhedronUnwrap(+mesh, cfg);
}
