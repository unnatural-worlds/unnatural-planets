#ifndef math_h_e6t4hjdr
#define math_h_e6t4hjdr

#include <cage-core/math.h>

using namespace cage;

template <class T>
inline T rescale(const T &v, real ia, real ib, real oa, real ob)
{
	return (v - ia) / (ib - ia) * (ob - oa) + oa;
}

real sharpEdge(real v, real p = 0.05);
vec3 colorDeviation(const vec3 &color, real deviation = 0.05);
vec3 normalDeviation(const vec3 &normal, real strength);
bool isUnit(const vec3 &v);
vec3 anyPerpendicular(const vec3 &a);
uint32 noiseSeed();

#endif
