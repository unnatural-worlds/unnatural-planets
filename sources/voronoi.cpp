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
	const Vec3i seedHashes;
	const Real frequency;

	VoronoiImpl(const VoronoiCreateConfig &cfg) : cfg(cfg), seedHashes(hash(cfg.seed), hash(hash(cfg.seed)), hash(hash(hash(cfg.seed)))), frequency(1 / cfg.cellSize)
	{}

	Vec3i mix(const Vec3i &s)
	{
		const Vec3i a = Vec3i(s[1], s[2], s[0]);
		const Vec3i b = Vec3i(hash(s[0]), hash(s[1]), hash(s[2]));
		return seedHashes + s + a + b;
	}

	Vec3 genPoint(const Vec3i &s)
	{
		return Vec3(s % 65536) / 65535;
	}

	void genCell(Vec3 *&out, const Vec3i &cell)
	{
		Vec3i s = mix(cell);
		for (uint32 i = 0; i < cfg.pointsPerCell; i++)
		{
			s = mix(s);
			*out++ = (genPoint(s) + Vec3(cell)) * cfg.cellSize;
		}
	}

	VoronoiResult evaluate(const Vec3 &position, const Vec3 &normal)
	{
		const uint32 totalPoints = cfg.pointsPerCell * 27;
		Vec3 *const pointsMem = (Vec3 *)ALLOCA(totalPoints * sizeof(Vec3));
		
		{ // generate all points (including neighboring cells)
			Vec3 *gen = pointsMem;
			const Vec3i cell = Vec3i(position * frequency);
			for (sint32 z = -1; z < 2; z++)
				for (sint32 y = -1; y < 2; y++)
					for (sint32 x = -1; x < 2; x++)
						genCell(gen, cell + Vec3i(x, y, z));
			CAGE_ASSERT(gen == pointsMem + totalPoints);
		}

		const PointerRange<Vec3> points = { pointsMem, pointsMem + totalPoints };

		{ // project all points into the plane
			const Plane pln = Plane(position, normal);
			for (Vec3 &p : points)
				p = closestPoint(pln, p);
		}

		{ // sort all points by distance
			std::sort(points.begin(), points.end(), [&](const Vec3 &a, const Vec3 &b) {
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

VoronoiResult Voronoi::evaluate(const Vec3 &position, const Vec3 &normal)
{
	VoronoiImpl *impl = (VoronoiImpl *)this;
	return impl->evaluate(position, normal);
}

Holder<Voronoi> newVoronoi(const VoronoiCreateConfig &cfg)
{
	return systemMemory().createImpl<Voronoi, VoronoiImpl>(cfg);
}
