#include <cage-core/color.h>
#include <cage-core/random.h>

#include "math.h"

real sharpEdge(real v, real p)
{
	return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
}

vec3 colorDeviation(const vec3 &color, real deviation)
{
	vec3 hsl = colorRgbToHsluv(color) + (randomChance3() - 0.5) * deviation;
	hsl[0] = (hsl[0] + 1) % 1;
	return colorHsluvToRgb(clamp(hsl, 0, 1));
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

namespace
{
	const uint32 globalSeed = (uint32)detail::getApplicationRandomGenerator().next();
}

uint32 noiseSeed()
{
	static uint32 offset = 0;
	offset++;
	return hash(globalSeed + offset);
}
