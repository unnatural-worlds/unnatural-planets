#include <cage-core/color.h>
#include <cage-core/random.h>

#include "math.h"

real sharpEdge(real v, real p)
{
	return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
}

real terrace(real x)
{
	// https://www.wolframalpha.com/input/?i=plot+sin%28%28x+-+round%28x%29%29+*+2.45%29%5E11+%2B+round%28x%29+%2C+x+%3D+0+to+5
	return pow(sin(rads((x - round(x)) * 2.45)), 11) + round(x);
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
	static RandomGenerator gen = detail::getApplicationRandomGenerator();
	return (uint32)gen.next();
}
