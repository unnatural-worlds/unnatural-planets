#include <algorithm> // std::sort

#include "voronoi.h"

#include <cage-core/geometry.h>
#include <cage-core/memoryAlloca.h>

namespace unnatural
{
	class VoronoiImpl : public Voronoi
	{
	public:
		const VoronoiCreateConfig cfg;
		const Vec3i seedHashes;
		const Real frequency;

		VoronoiImpl(const VoronoiCreateConfig &cfg) : cfg(cfg), seedHashes(hash(cfg.seed), hash(hash(cfg.seed)), hash(hash(hash(cfg.seed)))), frequency(1 / cfg.cellSize) {}

		Vec3i mix(const Vec3i &s)
		{
			const Vec3i a = Vec3i(s[1], s[2], s[0]);
			const Vec3i b = Vec3i(hash(s[0]), hash(s[1]), hash(s[2]));
			return seedHashes + s + a + b;
		}

		Vec3 genPoint(const Vec3i &s) { return Vec3(s % 65536) / 65535; }

		struct Point
		{
			Vec3 p; // point
			Real d; // distance
		};

		void genCell(Point *&out, const Vec3i &cell)
		{
			Vec3i s = mix(cell);
			for (uint32 i = 0; i < cfg.pointsPerCell; i++)
			{
				s = mix(s);
				(out++)->p = (genPoint(s) + Vec3(cell)) * cfg.cellSize;
			}
		}

		VoronoiResult evaluate(const Vec3 &position, const Vec3 &normal)
		{
			const uint32 totalPoints = cfg.pointsPerCell * 27;
			Point *const pointsMem = (Point *)CAGE_ALLOCA(totalPoints * sizeof(Point));

			{ // generate all points (including neighboring cells)
				Point *gen = pointsMem;
				const Vec3i cell = Vec3i(position * frequency);
				for (sint32 z = -1; z < 2; z++)
					for (sint32 y = -1; y < 2; y++)
						for (sint32 x = -1; x < 2; x++)
							genCell(gen, cell + Vec3i(x, y, z));
				CAGE_ASSERT(gen == pointsMem + totalPoints);
			}

			const PointerRange<Point> points = { pointsMem, pointsMem + totalPoints };

			if (valid(normal))
			{ // project all points into the plane
				const Plane pln = Plane(position, normal);
				for (Point &p : points)
					p.p = closestPoint(pln, p.p);
			}

			{ // sort all points by distance
				for (Point &p : points)
					p.d = distanceSquared(p.p, position);
				std::partial_sort(points.begin(), points.begin() + VoronoiResult::MaxPoints, points.end(), [&](const Point &a, const Point &b) { return a.d < b.d; });
			}

			VoronoiResult res;
			CAGE_ASSERT(VoronoiResult::MaxPoints <= points.size());
			for (uint32 i = 0; i < VoronoiResult::MaxPoints; i++)
				res.points[i] = points[i].p;
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
}
