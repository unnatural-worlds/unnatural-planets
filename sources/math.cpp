#include <cage-core/color.h>
#include <cage-core/random.h>

#include "math.h"

real rescale(real v, real ia, real ib, real oa, real ob)
{
	return (v - ia) / (ib - ia) * (ob - oa) + oa;
}

real sharpEdge(real v, real p)
{
	return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
}

real terrace(real x, real steepness)
{
	CAGE_ASSERT(steepness >= 0);
	const real f = floor(x);
	const real t = smootherstep(saturate((x - f) * max(steepness, 1))) + f;
	const real r = interpolate(x, t, saturate(steepness));
	CAGE_ASSERT(r > f - 1e-3 && r < f + 1 + 1e-3);
	return r;
}

real smoothMin(real a, real b, real k)
{
	// https://www.shadertoy.com/view/3ssGWj
	const real h = saturate((b - a) / k * 0.5 + 0.5);
	return interpolate(b, a, h) - k * h * (1 - h);
}

real smoothMax(real a, real b, real k)
{
	return -smoothMin(-a, -b, k);
}

vec3 colorDeviation(const vec3 &color, real deviation)
{
	vec3 hsl = colorRgbToHsluv(color) + (randomChance3() - 0.5) * deviation;
	hsl[0] = (hsl[0] + 1) % 1;
	return colorHsluvToRgb(saturate(hsl));
}

vec3 colorHueShift(const vec3 &rgb, real shift)
{
	vec3 hsv = colorRgbToHsv(rgb);
	hsv[0] = (hsv[0] + shift + 1) % 1;
	return colorHsvToRgb(hsv);
}

vec3 normalDeviation(const vec3 &normal, real strength)
{
	return normalize(normal + strength * randomDirection3());
}

bool isUnit(const vec3 &v)
{
	return abs(length(v) - 1) < 1e-3;
}

vec3 anyPerpendicular(const vec3 &a)
{
	CAGE_ASSERT(isUnit(a));
	vec3 b = vec3(1, 0, 0);
	if (abs(dot(a, b)) > 0.9)
		b = vec3(0, 1, 0);
	return normalize(cross(a, b));
}

uint32 noiseSeed()
{
	static RandomGenerator gen = detail::globalRandomGenerator();
	return (uint32)gen.next();
}
