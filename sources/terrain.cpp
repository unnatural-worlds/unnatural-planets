
#include "main.h"

#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>

namespace cage
{
	namespace detail
	{
		CAGE_API uint32 hash(uint32 key);
	}
}

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

	real terrainElevationImpl(const vec3 &pos)
	{
		vec3 p1 = pos;
		vec3 p2(p1[1], -p1[2], p1[0]);
		static const holder<noiseFunction> clouds1 = newClouds(globalSeed + 200, 3);
		static const holder<noiseFunction> clouds2 = newClouds(globalSeed + 201, 4);
		return pow(clouds1->evaluate(p1 * 5) * 0.5 + 0.5, 1.8) * pow(clouds2->evaluate(p2 * 8) * 0.5 + 0.5, 1.5);
	}

	real terrainElevation(const vec3 &pos)
	{
		return terrainElevationImpl(pos / planetScale);
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

	vec3 normalDeviation(const vec3 &normal, real strength)
	{
		return normalize(normal + strength * randomDirection3());
	}
}

real terrainDensity(const vec3 &pos)
{
	vec3 p1 = pos / planetScale;
	real e = terrainElevationImpl(p1);
	return (sphereDensity(p1) + e * 0.15);
}

void terrainPathProperties(const vec3 &pos, const vec3 &normal, uint32 &type, real &difficulty)
{
	real elev = terrainElevation(pos);
	static const real waterlineGlobal = (detail::hash(globalSeed + 15674) % 100) * 0.01 * 0.04 + 0.01;
	real waterline = waterlineGlobal;
	static const real snowlineGlobal = (detail::hash(globalSeed + 7154) % 100) * 0.01 * 0.2 + 0.2;
	static const holder<noiseFunction> snowlineClouds = newClouds(globalSeed + 44798, 2);
	real snowline = snowlineGlobal + (snowlineClouds->evaluate(pos * 1.345) - 0.5) * 0.03;
	if (elev < waterline)
	{
		type = 3;
		difficulty = elev / waterline;
	}
	else if (elev > snowline)
	{
		type = 5;
		difficulty = min(1, 2 * (elev - snowline) / (1 - snowline));
	}
	else
	{
		type = 0;
		difficulty = 0;
	}
}

void terrainMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
{
	uint32 type;
	real difficulty;
	terrainPathProperties(pos, normal, type, difficulty);
	switch (type)
	{
	case 3: // water
		albedo = colorDeviation(interpolate(pdnToRgb(235, 85, 48), pdnToRgb(199, 78, 78), difficulty), 0.05);
		special = vec2(randomRange(0.2, 0.3), 0.02);
		height = 0.05 * (1 - 0.7 * difficulty) * (randomChance() - 0.5) + 0.5;
		return;
	case 5: // snow
		albedo = colorDeviation(vec3(0.81), 0.02);
		special = vec2(randomRange(0.1, 0.4), 0.02);
		height = 0.5;
		return;
	}

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

	static const holder<noiseFunction> clouds1 = newClouds(globalSeed + 300, 8);
	real c = ((clouds1->evaluate(pos * 0.042) * 0.5 + 0.5) * 16) % 8;
	uint32 i = numeric_cast<uint32>(c);
	real f = sharpEdge(c - i);
	albedo = interpolate(colors[i], colors[(i + 1) % 8], f);
	albedo = recolor(albedo, 0.1, globalSeed + 548, pos);
	special = vec2(0.5, 0.02);
	height = 0.5;
}
