
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
	template <class T>
	T rescale(const T &v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	real sharpEdge(real v, real p = 0.05)
	{
		return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
	}

	vec3 pdnToRgb(real h, real s, real v)
	{
		return colorHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	vec3 colorDeviation(const vec3 &color, real deviation = 0.05)
	{
		vec3 hsv = colorRgbToHsv(color) + (randomChance3() - 0.5) * deviation;
		hsv[0] = (hsv[0] + 1) % 1;
		return colorHsvToRgb(clamp(hsv, vec3(0), vec3(1)));
	}

	vec3 normalDeviation(const vec3 &normal, real strength)
	{
		return normalize(normal + strength * randomDirection3());
	}

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

	void vegetationMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
	{
		static const vec3 colors[] = {
			pdnToRgb(151, 87, 30),
			pdnToRgb(93, 53, 45),
			pdnToRgb(116, 68, 71),
			pdnToRgb(70, 64, 76)
		};

		static const vec2 specials[] = {
			vec2(0.5, 0.02),
			vec2(0.3, 0.02),
			vec2(0.5, 0.02),
			vec2(0.3, 0.02)
		};

		static const holder<noiseFunction> clouds1 = newClouds(globalSeed + 98541, 4);
		static const holder<noiseFunction> clouds2 = newClouds(globalSeed + 13657, 6);
		static const holder<noiseFunction> clouds3 = newClouds(globalSeed + 36974, 4);
		static const holder<noiseFunction> clouds4 = newClouds(globalSeed + 21456, 6);
		real cs[4];
		cs[0] = clouds1->evaluate(pos * 0.08) + 0.2;
		cs[1] = clouds2->evaluate(pos * 0.34);
		cs[2] = clouds3->evaluate(pos * 0.08);
		cs[3] = clouds4->evaluate(pos * 0.57) - 0.1;
		uint32 i = 0;
		for (uint32 j = 1; j < 4; j++)
			if (cs[j] > cs[i])
				i = j;
		albedo = colorDeviation(colors[i]);
		special = specials[i];
		height = randomRange(0.45, 0.55);
	}

	void mountainRocksMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
	{
		static const vec3 colors[] = {
			pdnToRgb(66, 40, 62), // grass
			pdnToRgb(182, 32, 71), // metal
			pdnToRgb(218, 41, 34), // rock
			pdnToRgb(25, 43, 44) // dirt
		};

		static const vec2 specials[] = {
			vec2(0.3, 0.02),
			vec2(0.04, 0.55),
			vec2(0.5, 0.05),
			vec2(0.6, 0.03)
		};

		static const holder<noiseFunction> clouds1 = newClouds(globalSeed + 74892, 4);
		static const holder<noiseFunction> clouds2 = newClouds(globalSeed + 54747, 8);
		static const holder<noiseFunction> clouds3 = newClouds(globalSeed + 56744, 4);
		static const holder<noiseFunction> clouds4 = newClouds(globalSeed + 98323, 8);
		real cs[4];
		cs[0] = clouds1->evaluate(pos * 0.21);
		cs[1] = clouds2->evaluate(pos * 0.91) - 0.2;
		cs[2] = clouds3->evaluate(pos * 0.11) + 0.2;
		cs[3] = clouds4->evaluate(pos * 1.11);
		uint32 i = 0;
		for (uint32 j = 1; j < 4; j++)
			if (cs[j] > cs[i])
				i = j;
		albedo = colorDeviation(colors[i]);
		special = specials[i];
		height = randomRange(0.4, 0.6);
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
	CAGE_ASSERT(elev >= 0 && elev <= 1);
	
	{ // sea water
		static const real waterlineGlobal = (detail::hash(globalSeed + 15674) % 100) * 0.01 * 0.07 + 0.005;
		real waterline = waterlineGlobal;
		if (elev < waterline)
		{
			type = 3;
			difficulty = elev / waterline;
			return;
		}
		elev = (elev - waterline) / (1 - waterline);
		CAGE_ASSERT(elev >= 0 && elev <= 1);
	}

	{ // snow
		static const real snowlineGlobal = (detail::hash(globalSeed + 7154) % 100) * 0.01 * 0.25 + 0.2;
		static const holder<noiseFunction> snowlineClouds = newClouds(globalSeed + 44798, 5);
		real snowline = snowlineGlobal + (snowlineClouds->evaluate(pos * 1.345) - 0.5) * 0.03;
		if (elev > snowline)
		{
			type = 5;
			difficulty = min(1, 3 * (elev - snowline) / (1 - snowline));
			return;
		}
		elev = 1 - (snowline - elev) / snowline;
		CAGE_ASSERT(elev >= 0 && elev <= 1);
	}

	{ // mountain rocks
		static const real mountainGlobal = (detail::hash(globalSeed + 674) % 100) * 0.01 * 0.4 + 0.6;
		static const holder<noiseFunction> mountainClouds = newClouds(globalSeed + 789499, 3);
		real mountainline = mountainGlobal + (mountainClouds->evaluate(pos * 0.0954) - 0.5) * 0.2;
		if (elev > mountainline)
		{
			type = 4;
			difficulty = (elev - mountainline) / (1 - mountainline);
			return;
		}
		elev = 1 - (mountainline - elev) / mountainline;
		CAGE_ASSERT(elev >= 0 && elev <= 1);
	}

	{ // sand
		static const holder<noiseFunction> sandCloud1 = newClouds(globalSeed + 95670, 5);
		static const holder<noiseFunction> sandCloud2 = newClouds(globalSeed + 45677, 5);
		if (sandCloud1->evaluate(pos * 0.07) > 0.85 || sandCloud2->evaluate(pos * 0.13) + elev < 0.1)
		{
			type = 1;
			static const holder<noiseFunction> sandCloud3 = newClouds(globalSeed + 688879, 5);
			difficulty = pow(sandCloud3->evaluate(pos * 0.274), 1.3);
			return;
		}
	}

	{ // vegetation
		static const holder<noiseFunction> vegetationClouds = newClouds(globalSeed + 3547746, 5);
		type = 2;
		difficulty = pow(vegetationClouds->evaluate(pos * 0.1374), 1.3);
	}
}

void terrainMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
{
	uint32 type;
	real difficulty;
	terrainPathProperties(pos, normal, type, difficulty);
	static const holder<noiseFunction> rocksClouds = newClouds(globalSeed + 68971, 4);
	switch (type)
	{
	case 0: // concrete
		// todo
		break; //return;
	case 1: // sand
		albedo = colorDeviation(interpolateColor(pdnToRgb(55, 99, 97), pdnToRgb(44, 70, 74), difficulty), 0.02);
		special = vec2(randomRange(0.4, 0.8), randomRange(0.01, 0.2));
		height = randomRange(0.47, 0.53);
		return;
	case 2: // vegetation
		vegetationMaterial(pos, normal, albedo, special, height);
		return;
	case 3: // sea water
		albedo = colorDeviation(interpolateColor(pdnToRgb(235, 85, 48), pdnToRgb(199, 78, 78), difficulty));
		special = vec2(randomRange(0.2, 0.3), 0.02);
		height = 0.05 * (1 - 0.7 * difficulty) * (randomChance() - 0.5) + 0.5;
		return;
	case 4: // mountain rocks
		mountainRocksMaterial(pos, normal, albedo, special, height);
		return;
	case 5: // snow
		albedo = colorDeviation(vec3(0.81), 0.02);
		special = vec2(randomRange(0.1, 0.4), 0.02);
		height = randomRange(0.49, 0.51);
		return;
	}

	// default
	albedo = vec3(0.5);
	special = vec2(0.5, 0);
	height = 0.5;
}
