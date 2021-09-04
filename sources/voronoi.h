#ifndef voronoi_h_sdrt859
#define voronoi_h_sdrt859

#include <cage-core/math.h>

using namespace cage;

struct VoronoiResult
{
	static constexpr uint32 MaxPoints = 4;
	Vec3 points[MaxPoints];
};

class Voronoi : private Immovable
{
public:
	VoronoiResult evaluate(const Vec3 &position, const Vec3 &normal);
};

struct VoronoiCreateConfig
{
	uint32 seed = 0;
	uint32 pointsPerCell = 1;
	Real cellSize = 10;
};

Holder<Voronoi> newVoronoi(const VoronoiCreateConfig &cfg);

#endif
