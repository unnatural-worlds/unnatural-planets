
#include "main.h"

#include <cage-core/noiseFunction.h>
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

	real sphereDensity(const vec3 &pos)
	{
		return real(0.7) - length(pos);
	}
}

real terrainDensity(const vec3 &pos)
{
	vec3 p2(pos[1], -pos[2], pos[0]);
	static holder<noiseFunction> clouds1 = newClouds(globalSeed + 200, 3);
	static holder<noiseFunction> clouds2 = newClouds(globalSeed + 201, 3);
	return sphereDensity(pos) + (clouds1->evaluate(pos * 13) * clouds2->evaluate(p2 * 17)) * 0.05;
}
