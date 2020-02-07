#include "generator.h"

#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>

namespace
{
	uint32 globalSeed = (uint32)detail::getApplicationRandomGenerator().next();

	//---------
	// general
	//---------

	uint32 noiseSeed()
	{
		static uint32 offset = 0;
		offset += globalSeed % 123;
		return globalSeed + offset;
	}

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
		vec3 hsl = colorRgbToHsluv(color) + (randomChance3() - 0.5) * deviation;
		hsl[0] = (hsl[0] + 1) % 1;
		return colorHsluvToRgb(clamp(hsl, 0, 1));
	}

	vec3 normalDeviation(const vec3 &normal, real strength)
	{
		return normalize(normal + strength * randomDirection3());
	}

	Holder<NoiseFunction> newClouds(uint32 seed, uint32 octaves)
	{
		NoiseFunctionCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = NoiseTypeEnum::Value;
		cfg.seed = seed;
		return newNoiseFunction(cfg);
	}

	//-----------
	// densities
	//-----------

	real densitySphere(const vec3 &pos)
	{
		return 55 - length(pos);
	}

	//--------
	// biomes
	//--------

	// inspired by Whittaker diagram
	enum class BiomeEnum
	{
		Ocean,
		Beach,
		Snow,
		Tundra,
		Bare,
		Scorched,
		Taiga,
		Shrubland,
		TemperateDesert,
		TemperateRainForest,
		TemperateDeciduousForest,
		Grassland,
		TropicalRainForest,
		TropicalSeasonalForest,
		SubtropicalDesert,
	};

	BiomeEnum biome(real elevation, real moisture)
	{
		elevation = pow(elevation, 0.8);
		// https://www.redblobgames.com/maps/terrain-from-noise/
		if (elevation < 0.1)
			return BiomeEnum::Ocean;
		if (elevation < 0.12)
			return BiomeEnum::Beach;
		if (elevation > 0.8)
		{
			if (moisture < 0.1)
				return BiomeEnum::Scorched;
			if (moisture < 0.2)
				return BiomeEnum::Bare;
			if (moisture < 0.5)
				return BiomeEnum::Tundra;
			return BiomeEnum::Snow;
		}
		if (elevation > 0.6)
		{
			if (moisture < 0.33)
				return BiomeEnum::TemperateDesert;
			if (moisture < 0.66)
				return BiomeEnum::Shrubland;
			return BiomeEnum::Taiga;
		}
		if (elevation > 0.3)
		{
			if (moisture < 0.16)
				return BiomeEnum::TemperateDesert;
			if (moisture < 0.50)
				return BiomeEnum::Grassland;
			if (moisture < 0.83)
				return BiomeEnum::TemperateDeciduousForest;
			return BiomeEnum::TemperateRainForest;
		}
		if (moisture < 0.16)
			return BiomeEnum::SubtropicalDesert;
		if (moisture < 0.33)
			return BiomeEnum::Grassland;
		if (moisture < 0.66)
			return BiomeEnum::TropicalSeasonalForest;
		return BiomeEnum::TropicalRainForest;
	}

	vec3 biomeColor(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean: return vec3(54, 54, 97) / 255;
		case BiomeEnum::Beach: return vec3(172, 159, 139) / 255;
		case BiomeEnum::Snow: return vec3(248, 248, 248) / 255;
		case BiomeEnum::Tundra: return vec3(221, 221, 187) / 255;
		case BiomeEnum::Bare: return vec3(187, 187, 187) / 255;
		case BiomeEnum::Scorched: return vec3(153, 153, 153) / 255;
		case BiomeEnum::Taiga: return vec3(204, 212, 187) / 255;
		case BiomeEnum::Shrubland: return vec3(196, 204, 187) / 255;
		case BiomeEnum::TemperateDesert: return vec3(228, 232, 202) / 255;
		case BiomeEnum::TemperateRainForest: return vec3(164, 196, 168) / 255;
		case BiomeEnum::TemperateDeciduousForest: return vec3(180, 201, 169) / 255;
		case BiomeEnum::Grassland: return vec3(196, 212, 170) / 255;
		case BiomeEnum::TropicalRainForest: return vec3(156, 187, 169) / 255;
		case BiomeEnum::TropicalSeasonalForest: return vec3(169, 204, 164) / 255;
		case BiomeEnum::SubtropicalDesert: return vec3(233, 221, 199) / 255;
		default: return vec3::Nan();
		};
	}

	uint8 biomeTerrainType(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean:
			return 8; // water
		case BiomeEnum::Bare:
		case BiomeEnum::Scorched:
		case BiomeEnum::Grassland:
		case BiomeEnum::TemperateDesert:
		case BiomeEnum::TemperateRainForest:
		case BiomeEnum::TemperateDeciduousForest:
		case BiomeEnum::TropicalSeasonalForest:
			return 4; // fast
		case BiomeEnum::Snow:
		case BiomeEnum::Beach:
		case BiomeEnum::Tundra:
		case BiomeEnum::Taiga:
		case BiomeEnum::Shrubland:
		case BiomeEnum::TropicalRainForest:
		case BiomeEnum::SubtropicalDesert:
			return 2; // slow
		default:
			return 0;
		}
	}

	//-----------
	// elevation
	//-----------

	real terrainElevation(const vec3 &pos)
	{
		static const Holder<NoiseFunction> clouds1 = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 3;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> clouds2 = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		vec3 p1 = pos * 0.01;
		vec3 p2(p1[1], -p1[2], p1[0]);
		return pow(clouds1->evaluate(p1 * 5) * 0.5 + 0.5, 1.8) * pow(clouds2->evaluate(p2 * 8) * 0.5 + 0.5, 1.5);
	}

	//----------
	// moisture
	//----------

	real terrainMoisture(const vec3 &pos, const vec3 &normal)
	{
		static const Holder<NoiseFunction> clouds1 = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> clouds2 = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 3;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		return clouds1->evaluate(pos * 0.1 + normal * clouds2->evaluate(pos * 0.2)) * 0.5 + 0.5;
	}

	//---------
	// terrain
	//---------

	void terrain(const vec3 &pos, const vec3 &normal, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		real elev = terrainElevation(pos);
		CAGE_ASSERT(elev >= 0 && elev <= 1);
		real moist = terrainMoisture(pos, normal);
		CAGE_ASSERT(moist >= 0 && moist <= 1);
		real temp = 1 - elev;
		//temp *= 1 - pow(abs(pos[1] / length(pos)), 5); // colder on poles
		CAGE_ASSERT(temp >= 0 && temp <= 1);
		BiomeEnum biom = biome(1 - temp, moist);
		terrainType = biomeTerrainType(biom);
		albedo = biomeColor(biom);
		special = vec2(0.5);
		height = 0;
	}
}

real functionDensity(const vec3 &pos)
{
	return densitySphere(pos) + terrainElevation(pos) * 15;
}

void functionTileProperties(const vec3 &pos, const vec3 &normal, uint8 &terrainType)
{
	vec3 albedo;
	vec2 special;
	real height;
	terrain(pos, normal, terrainType, albedo, special, height);
}

void functionMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
{
	uint8 terrainType;
	terrain(pos, normal, terrainType, albedo, special, height);
	albedo = colorDeviation(albedo, 0.01);
}
