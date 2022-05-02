#include <cage-core/color.h>
#include <cage-core/random.h>

#include "math.h"

Real rescale(Real v, Real ia, Real ib, Real oa, Real ob)
{
	return (v - ia) / (ib - ia) * (ob - oa) + oa;
}

Real sharpEdge(Real v, Real p)
{
	return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
}

Real terrace(Real x, Real steepness)
{
	CAGE_ASSERT(steepness >= 0);
	const Real f = floor(x);
	const Real t = smootherstep(saturate((x - f) * max(steepness, 1))) + f;
	const Real r = interpolate(x, t, saturate(steepness));
	CAGE_ASSERT(r > f - 1e-3 && r < f + 1 + 1e-3);
	return r;
}

Real smoothMin(Real a, Real b, Real k)
{
	// https://www.shadertoy.com/view/3ssGWj
	const Real h = saturate((b - a) / k * 0.5 + 0.5);
	return interpolate(b, a, h) - k * h * (1 - h);
}

Real smoothMax(Real a, Real b, Real k)
{
	return -smoothMin(-a, -b, k);
}

Vec3 colorDeviation(const Vec3 &color, Real deviation)
{
	Vec3 hsl = colorRgbToHsluv(color) + (randomChance3() - 0.5) * deviation;
	hsl[0] = (hsl[0] + 1) % 1;
	return colorHsluvToRgb(saturate(hsl));
}

Vec3 colorHueShift(const Vec3 &rgb, Real shift)
{
	Vec3 hsv = colorRgbToHsv(rgb);
	hsv[0] = (hsv[0] + shift + 1) % 1;
	return colorHsvToRgb(hsv);
}

Vec3 normalDeviation(const Vec3 &normal, Real strength)
{
	return normalize(normal + strength * randomDirection3());
}

bool isUnit(const Vec3 &v)
{
	return abs(length(v) - 1) < 1e-3;
}

Vec3 anyPerpendicular(const Vec3 &a)
{
	CAGE_ASSERT(isUnit(a));
	Vec3 b = Vec3(1, 0, 0);
	if (abs(dot(a, b)) > 0.9)
		b = Vec3(0, 1, 0);
	return normalize(cross(a, b));
}

uint32 noiseSeed()
{
	static RandomGenerator gen = detail::randomGenerator();
	return (uint32)gen.next();
}
