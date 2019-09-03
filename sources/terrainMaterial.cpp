
#include "main.h"

#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>

namespace
{
	holder<noiseFunction> newClouds(uint32 seed, uint32 octaves)
	{
		noiseFunctionCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = noiseTypeEnum::Value;
		cfg.seed = seed;
		return newNoiseFunction(cfg);
	}

	vec3 pdnToRgb(real h, real s, real v)
	{
		return convertHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	vec3 recolor(const vec3 &color, real deviation, uint32 seed, const vec3 &pos)
	{
		static holder<noiseFunction> value1 = newClouds(globalSeed + 100, 1);
		static holder<noiseFunction> value2 = newClouds(globalSeed + 101, 1);
		real h = (value1->evaluate(pos * 3) * 0.5 + 0.5) * 0.5 + 0.25;
		real v = (value2->evaluate(pos * 4) * 0.5 + 0.5);
		vec3 hsv = convertRgbToHsv(color) + (vec3(h, 1 - v, v) - 0.5) * deviation;
		hsv[0] = (hsv[0] + 1) % 1;
		return convertHsvToRgb(clamp(hsv, vec3(), vec3(1)));
	}

	vec3 colorDeviation(const vec3 &color, real deviation)
	{
		vec3 hsv = convertRgbToHsv(color) + (randomChance3() - 0.5) * deviation;
		hsv[0] = (hsv[0] + 1) % 1;
		return convertHsvToRgb(clamp(hsv, vec3(), vec3(1)));
	}
}

void terrainMaterial(const vec3 &pos, vec3 &albedo, vec2 &special)
{
	static const vec3 colors[] = {
		pdnToRgb(240, 1, 45),
		pdnToRgb(230, 6, 35),
		pdnToRgb(240, 11, 28),
		pdnToRgb(232, 27, 21),
		pdnToRgb(31, 34, 96),
		pdnToRgb(31, 56, 93),
		pdnToRgb(26, 68, 80),
		pdnToRgb(21, 69, 55)
	};

	static holder<noiseFunction> clouds1 = newClouds(globalSeed + 300, 8);
	real c = ((clouds1->evaluate(pos * 0.042) * 0.5 + 0.5) * 16) % 8;
	uint32 i = numeric_cast<uint32>(c);
	real f = sharpEdge(c - i);
	albedo = interpolate(colors[i], colors[(i + 1) % 8], f);
	albedo = recolor(albedo, 0.1, globalSeed + 548, pos);
	special = vec2(0.5, 0.02);
}
