#include <cage-core/geometry.h>

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
	const uint32 iterations = 1;
	const float targetScale = 3;
#else
	const uint32 iterations = 10;
	const float targetScale = 1;
#endif // CAGE_DEBUG
}

void meshSimplifyRegular(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing mesh");
	OPTICK_EVENT();

	PolyhedronRegularizationConfig cfg;
	cfg.iterations = iterations;
	cfg.targetEdgeLength = targetScale;
	mesh->regularize(cfg);
}

void meshSimplifyDynamic(Holder<Polyhedron> &mesh)
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
		mesh = templates::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified mesh has more triangles than the original");
}

void meshDiscardDisconnected(Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "detecting disconnected parts");
	OPTICK_EVENT();
	mesh->discardDisconnected();
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

real meshAverageEdgeLength(const Holder<Polyhedron> &mesh)
{
	real len = 0;
	const uint32 trisCnt = mesh->indicesCount() / 3;
	for (uint32 tri = 0; tri < trisCnt; tri++)
	{
		vec3 a = mesh->position(mesh->index(tri * 3 + 0));
		vec3 b = mesh->position(mesh->index(tri * 3 + 1));
		vec3 c = mesh->position(mesh->index(tri * 3 + 2));
		len += distance(a, b);
		len += distance(b, c);
		len += distance(c, a);
	}
	return len / mesh->indicesCount();
}
