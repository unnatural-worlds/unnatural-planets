#include <cage-core/geometry.h>

#include "voronoi.h"

#include <algorithm> // std::sort

#ifdef _MSC_VER
#include <malloc.h>
#define ALLOCA _alloca
#else
#define ALLOCA __builtin_alloca
#endif // _MSC_VER

class VoronoiImpl : public Voronoi
{
public:
	const VoronoiCreateConfig cfg;
	const ivec3 seedHashes;
	const real frequency;

	VoronoiImpl(const VoronoiCreateConfig &cfg) : cfg(cfg), seedHashes(hash(cfg.seed), hash(hash(cfg.seed)), hash(hash(hash(cfg.seed)))), frequency(1 / cfg.cellSize)
	{}

	ivec3 mix(const ivec3 &s)
	{
		const ivec3 a = ivec3(s[1], s[2], s[0]);
		const ivec3 b = ivec3(hash(s[0]), hash(s[1]), hash(s[2]));
		return seedHashes + s + a + b;
	}

	vec3 genPoint(const ivec3 &s)
	{
		return vec3(s % 65536) / 65535;
	}

	void genCell(vec3 *&out, const ivec3 &cell)
	{
		ivec3 s = mix(cell);
		for (uint32 i = 0; i < cfg.pointsPerCell; i++)
		{
			s = mix(s);
			*out++ = (genPoint(s) + vec3(cell)) * cfg.cellSize;
		}
	}

	VoronoiResult evaluate(const vec3 &position, const vec3 &normal)
	{
		const uint32 totalPoints = cfg.pointsPerCell * 27;
		vec3 *const pointsMem = (vec3 *)ALLOCA(totalPoints * sizeof(vec3));
		
		{ // generate all points (including neighboring cells)
			vec3 *gen = pointsMem;
			const ivec3 cell = ivec3(position * frequency);
			for (sint32 z = -1; z < 2; z++)
				for (sint32 y = -1; y < 2; y++)
					for (sint32 x = -1; x < 2; x++)
						genCell(gen, cell + ivec3(x, y, z));
			CAGE_ASSERT(gen == pointsMem + totalPoints);
		}

		const PointerRange<vec3> points = { pointsMem, pointsMem + totalPoints };

		{ // project all points into the plane
			const Plane pln = Plane(position, normal);
			for (vec3 &p : points)
				p = closestPoint(pln, p);
		}

		{ // sort all points by distance
			std::sort(points.begin(), points.end(), [&](const vec3 &a, const vec3 &b) {
				return distanceSquared(a, position) < distanceSquared(b, position);
				});
		}

		VoronoiResult res;
		CAGE_ASSERT(VoronoiResult::MaxPoints <= points.size());
		for (uint32 i = 0; i < VoronoiResult::MaxPoints; i++)
			res.points[i] = points[i];
		return res;
	}
};

VoronoiResult Voronoi::evaluate(const vec3 &position, const vec3 &normal)
{
	VoronoiImpl *impl = (VoronoiImpl *)this;
	return impl->evaluate(position, normal);
}

Holder<Voronoi> newVoronoi(const VoronoiCreateConfig &cfg)
{
	return systemMemory().createImpl<Voronoi, VoronoiImpl>(cfg);
}
