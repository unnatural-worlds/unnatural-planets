#ifndef math_h_e6t4hjdr
#define math_h_e6t4hjdr

#include <cage-core/math.h>

using namespace cage;

Real rescale(Real v, Real ia, Real ib, Real oa, Real ob);
Real sharpEdge(Real v, Real p = 0.05);
Real terrace(Real x, Real steepness);
Real smoothMin(Real a, Real b, Real k);
Real smoothMax(Real a, Real b, Real k);
Vec3 colorDeviation(const Vec3 &color, Real deviation = 0.05);
Vec3 colorHueShift(const Vec3 &rgb, Real shift);
Vec3 normalDeviation(const Vec3 &normal, Real strength);
bool isUnit(const Vec3 &v);
Vec3 anyPerpendicular(const Vec3 &a);
uint32 noiseSeed();

#endif
