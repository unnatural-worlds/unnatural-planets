
#include "main.h"

#include <cage-core/noise.h>
#include <cage-core/random.h>

namespace
{
	holder<noiseClass> newClouds(uint32 seed, uint32 octaves)
	{
		noiseCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = noiseTypeEnum::Value;
		cfg.seed = seed;
		return newNoise(cfg);
	}

	real sphereDensity(const vec3 &pos)
	{
		return 0.7 - pos.length();
	}
}

real terrainDensity(const vec3 &pos)
{
	vec3 p2(pos[1], -pos[2], pos[0]);
	static holder<noiseClass> clouds1 = newClouds(globalSeed + 200, 3);
	static holder<noiseClass> clouds2 = newClouds(globalSeed + 201, 3);
	return sphereDensity(pos) + (clouds1->evaluate(pos * 13) * clouds2->evaluate(p2 * 17)) * 0.05;
}
