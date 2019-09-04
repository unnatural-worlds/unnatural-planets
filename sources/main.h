#ifndef main_h_dsfg4ds5
#define main_h_dsfg4ds5

#include <cage-core/core.h>
#include <cage-core/log.h>
#include <cage-core/math.h>

using namespace cage;

extern uint32 globalSeed;
extern real planetScale;

real terrainDensity(const vec3 &pos);
void terrainMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special);
void terrainProperties(const vec3 &pos, const vec3 &normal, uint32 &type, real &difficulty);

void generateTerrain();
void exportTerrain();


template <class T>
T rescale(const T &v, real ia, real ib, real oa, real ob)
{
	return (v - ia) / (ib - ia) * (ob - oa) + oa;
}

inline real sharpEdge(real v, real p = 0.05)
{
	return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
}

#endif
