#ifndef main_h_dsfg4ds5
#define main_h_dsfg4ds5

#include <cage-core/core.h>
#include <cage-core/math.h>

using namespace cage;

extern uint32 globalSeed;
extern real planetScale;

real terrainDensity(const vec3 &pos);
void terrainPathProperties(const vec3 &pos, const vec3 &normal, uint32 &type, real &difficulty);
void terrainMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height);

void generateTerrain();
void exportTerrain();

#endif
