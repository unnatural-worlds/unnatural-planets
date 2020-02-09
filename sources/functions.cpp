#include "generator.h"

#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>

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

	//-----------
	// densities
	//-----------

	real densitySphere(const vec3 &pos)
	{
		return 60 - length(pos);
	}

	real densityTorus(const vec3 &pos)
	{
		vec3 c = normalize(pos * vec3(1, 0, 1)) * 70;
		return 25 - distance(pos, c);
	}

	real densityPretzel(const vec3 &pos)
	{
		vec3 c = normalize(pos * vec3(1, 0, 1));
		rads yaw = atan2(c[0], c[2]);
		rads pitch = atan2(pos[1], 70 - length(vec2(pos[0], pos[2])));
		rads ang = yaw + pitch;
		real t = length(vec2(sin(ang) * 25, cos(ang) * 5));
		real l = distance(pos, c * 70);
		return t - l;
	}

	real densityMobiusStrip(const vec3 &pos)
	{
		// todo fix this
		vec3 c = normalize(pos * vec3(1, 0, 1));
		rads yaw = atan2(c[0], c[2]);
		rads pitch = atan2(length(vec2(pos[0], pos[2])) - 70, pos[1]);
		rads ang = pitch;
		real si = sin(ang);
		real co = cos(ang);
		real x = abs(co) * co + abs(si) * si;
		real y = abs(co) * co - abs(si) * si;
		real t = length(vec2(x * 25, y * 5));
		real l = distance(pos, c * 70);
		return t - l;
	}

	bool sideOfAPlane(const triangle &tri, const vec3 &pt)
	{
		plane pl(tri);
		return dot(pl.normal, pt) < -pl.d;
	}

	real densityTetrahedron(const vec3 &pos)
	{
		const real size = 70;
		const vec3 corners[4] = { vec3(1,1,1)*size, vec3(1,-1,-1)*size, vec3(-1,1,-1)*size, vec3(-1,-1,1)*size };
		const triangle tris[4] = {
			triangle(corners[0], corners[1], corners[2]),
			triangle(corners[0], corners[3], corners[1]),
			triangle(corners[0], corners[2], corners[3]),
			triangle(corners[1], corners[3], corners[2])
		};
		const bool insides[4] = {
			sideOfAPlane(tris[0], pos),
			sideOfAPlane(tris[1], pos),
			sideOfAPlane(tris[2], pos),
			sideOfAPlane(tris[3], pos)
		};
		const real lens[4] = {
			distance(tris[0], pos),
			distance(tris[1], pos),
			distance(tris[2], pos),
			distance(tris[3], pos)
		};
		if (insides[0] && insides[1] && insides[2] && insides[3])
		{
			real r = min(min(lens[0], lens[1]), min(lens[2], lens[3]));
			CAGE_ASSERT(r.valid() && r.finite());
			return r;
		}
		else
		{
			real r = real::Infinity();
			for (int i = 0; i < 4; i++)
			{
				if (!insides[i])
					r = min(r, lens[i]);
			}
			CAGE_ASSERT(r.valid() && r.finite());
			return -r;
		}
	}

	real baseShapeDensity(const vec3 &pos)
	{
		return densitySphere(pos);
	}

	//--------
	// biomes
	//--------

	// inspired by Whittaker diagram
	enum class BiomeEnum
	{
		Ocean,
		Ice,
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

	BiomeEnum biome(real elevation, real temperature, real moisture)
	{
		if (elevation < 0.06)
		{
			if (temperature < 0.1)
				return BiomeEnum::Ice;
			return BiomeEnum::Ocean;
		}
		if (elevation < 0.07)
		{
			if (temperature < 0.2)
				return BiomeEnum::Ice;
			return BiomeEnum::Beach;
		}
		if (temperature < 0.2)
		{
			if (moisture < 0.1)
				return BiomeEnum::Scorched;
			if (moisture < 0.2)
				return BiomeEnum::Bare;
			if (moisture < 0.5)
				return BiomeEnum::Tundra;
			return BiomeEnum::Snow;
		}
		if (temperature < 0.4)
		{
			if (moisture < 0.33)
				return BiomeEnum::TemperateDesert;
			if (moisture < 0.66)
				return BiomeEnum::Shrubland;
			return BiomeEnum::Taiga;
		}
		if (temperature < 0.7)
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
		case BiomeEnum::Ice: return vec3(130, 186, 233) / 255;
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
		case BiomeEnum::Ice:
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
		return clouds1->evaluate(pos * 0.05 + normal * clouds2->evaluate(pos * 0.2) * 0.5) * 0.5 + 0.5;
	}

	//---------
	// terrain
	//---------

	void terrain(const vec3 &pos, const vec3 &normal, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> elevationOffsetClouds = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> temperatureOffsetClouds = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real elev = terrainElevation(pos);
		elev += elevationOffsetClouds->evaluate(pos * 0.3) * 0.05;
		real moist = terrainMoisture(pos, normal);
		real temp = 1 - pow(elev, 0.8);
		real polar = pow(abs(pos[1] / length(pos)), 6);
		temp *= 1 - polar;
		moist += polar * 0.3;
		temp += temperatureOffsetClouds->evaluate(pos * 0.1) * 0.15;
		BiomeEnum biom = biome(elev, temp, moist);
		terrainType = biomeTerrainType(biom);
		albedo = biomeColor(biom);
		special = vec2(0.5);
		height = 0;
	}
}

real functionDensity(const vec3 &pos)
{
	return baseShapeDensity(pos) + terrainElevation(pos) * 15;
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
	albedo = colorDeviation(albedo, 0.03);
}
