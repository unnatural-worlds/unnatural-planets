#ifndef functions_h_4hges9
#define functions_h_4hges9

#include "common.h"

real functionDensity(const vec3 &pos);
void functionTileProperties(const vec3 &pos, const vec3 &normal, uint8 &terrainType);
void functionMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height);

#endif
