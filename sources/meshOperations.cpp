#include <cage-core/geometry.h>
#include <cage-core/ini.h>

#include <unnatural-navmesh/navmesh.h>

#include "mesh.h"

#include <initializer_list>

namespace
{
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
		vec3 c = box.center();
		vec3 hs = box.size() * 0.6; // slightly larger box to avoid clipping due to floating point imprecisions
		aabb r = aabb(c - hs, c + hs);
		if (second)
			r.a[axis] = pos;
		else
			r.b[axis] = pos;
		return r;
	}

#ifdef CAGE_DEBUG
	constexpr uint32 iterations = 1;
	constexpr float targetScale = 3;
#else
	constexpr uint32 iterations = 10;
	constexpr float targetScale = 1;
#endif // CAGE_DEBUG

	bool navmeshOptimize = true;
}

void meshSimplifyNavmesh(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing navigation mesh");
	OPTICK_EVENT();

	PolyhedronRegularizationConfig reg;
	reg.iterations = iterations;
	reg.targetEdgeLength = targetScale;
	if (navmeshOptimize)
	{
		unnatural::NavmeshOptimizationConfig cfg;
		cfg.regularization = reg;
		// todo other configuration
		mesh = unnatural::navmeshOptimize(templates::move(mesh), cfg);
	}
	else
		mesh->regularize(reg);
}

void meshSimplifyCollider(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying collider mesh");
	OPTICK_EVENT();

	PolyhedronSimplificationConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.5 * targetScale;
	cfg.maxEdgeLength = 10 * targetScale;
	cfg.approximateError = 0.03 * targetScale;
	Holder<Polyhedron> m = mesh->copy();
	m->simplify(cfg);

	if (m->indicesCount() < mesh->indicesCount())
		mesh = templates::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified collider mesh has more triangles than the original");
}

void meshSimplifyRender(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying render mesh");
	OPTICK_EVENT();

	PolyhedronSimplificationConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.5 * targetScale;
	cfg.maxEdgeLength = 10 * targetScale;
	cfg.approximateError = 0.03 * targetScale;
	Holder<Polyhedron> m = mesh->copy();
	m->simplify(cfg);

	if (m->indicesCount() < mesh->indicesCount())
		mesh = templates::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified render mesh has more triangles than the original");
}

SplitResult meshSplit(const Holder<Polyhedron> &mesh)
{
	OPTICK_EVENT();
	const real myArea = meshSurfaceArea(mesh.get());
	OPTICK_TAG("area", myArea.value);
	SplitResult result;
	if (myArea > 2500)
	{
		const aabb myBox = mesh->boundingBox();
		const uint32 a = boxLongestAxis(myBox);
		real bestSplitPosition = 0.5;
		real bestSplitScore = real::Infinity();
		for (real position : { 0.3, 0.4, 0.45, 0.5, 0.55, 0.6, 0.7 })
		{
			Holder<Polyhedron> p = mesh->copy();
			p->clip(clippingBox(myBox, a, interpolate(myBox.a[a], myBox.b[a], position)));
			real area = meshSurfaceArea(p.get());
			real score = abs(0.5 - area / myArea);
			if (score < bestSplitScore)
			{
				bestSplitScore = score;
				bestSplitPosition = position;
			}
		}
		real split = interpolate(myBox.a[a], myBox.b[a], bestSplitPosition);
		Holder<Polyhedron> m1 = mesh->copy();
		Holder<Polyhedron> m2 = mesh->copy();
		m1->clip(clippingBox(myBox, a, split));
		m2->clip(clippingBox(myBox, a, split, true));
		result = meshSplit(m1);
		SplitResult r2 = meshSplit(m2);
		for (auto &it : r2.meshes)
			result.meshes.push_back(templates::move(it));
	}
	else
	{
		// no more splitting is required
		result.meshes.push_back(mesh->copy());
	}
	return result;
}

uint32 meshUnwrap(const Holder<Polyhedron> &mesh)
{
	OPTICK_EVENT("unwrapping");
	PolyhedronUnwrapConfig cfg;
	cfg.maxChartIterations = 10;
	cfg.maxChartBoundaryLength = 50;
	cfg.chartRoundness = 0.3;
#ifdef CAGE_DEBUG
	cfg.texelsPerUnit = 1;
#else
	cfg.texelsPerUnit = 20;
#endif // CAGE_DEBUG
	cfg.padding = 6;
	return mesh->unwrap(cfg);
}

void meshConfigure(const Holder<Ini> &cmd)
{
	{ // optimizations
		navmeshOptimize = cmd->cmdBool('o', "optimize", navmeshOptimize);
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "navmesh optimizations: " + navmeshOptimize);
	}
}
