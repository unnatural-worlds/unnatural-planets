#ifndef math_h_e6t4hjdr
#define math_h_e6t4hjdr

#include <cage-core/math.h>

using namespace cage;

real rescale(real v, real ia, real ib, real oa, real ob);
real sharpEdge(real v, real p = 0.05);
real terrace(real x, real steepness);
real smoothMin(real a, real b, real k);
real smoothMax(real a, real b, real k);
vec3 colorDeviation(const vec3 &color, real deviation = 0.05);
vec3 colorHueShift(const vec3 &rgb, real shift);
vec3 normalDeviation(const vec3 &normal, real strength);
bool isUnit(const vec3 &v);
vec3 anyPerpendicular(const vec3 &a);
uint32 noiseSeed();

#endif
