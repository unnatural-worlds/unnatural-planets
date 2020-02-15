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
		const auto &sideOfAPlane = [](const triangle &tri, const vec3 &pt) -> bool
		{
			plane pl(tri);
			return dot(pl.normal, pt) < -pl.d;
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
		                          // temperature // precipitation // coverage //
		                          //    (°C)     //     (cm)      //   (%)    //
		Ocean,                    //             //               //          //
		Ice,                      //             //               //          //
		Beach,                    //             //               //          //
		Slope,                    //             //               //          //
		Snow,                     //     ..  5   //   30 ..       //          //
		Bare,                     // -15 .. -5   //    0 ..  10   //          //
		Tundra,                   // -15 .. -5   //   10 ..  30   //    11    //
		Taiga,                    //  -5 ..  5   //   30 .. 150   //    17    // (BorealForest)
		Shrubland,                //   5 .. 20   //    0 ..  30   //     3    //
		Grassland,                //   5 .. 20   //   30 .. 100   //    13    //
		TemperateSeasonalForest,  //   5 .. 20   //  100 .. 200   //     4    // (TemperateDeciduousForest)
		TemperateRainForest,      //   5 .. 20   //  200 .. 300   //     4    //
		Desert,                   //  20 .. 30   //    0 ..  40   //    19    // (SubtropicalDesert)
		Savanna,                  //  20 .. 30   //   40 .. 130   //    10    // (TropicalGrassland)
		TropicalSeasonalForest,   //  20 .. 30   //  130 .. 230   //     6    //
		TropicalRainForest,       //  20 .. 30   //  230 .. 440   //     6    //
	};

	BiomeEnum biome(real elev, rads slope, real temp, real precp)
	{
		if (elev < 0)
		{
			if (temp < 0)
				return BiomeEnum::Ice;
			return BiomeEnum::Ocean;
		}
		if (slope > degs(40))
			return BiomeEnum::Slope;
		if (elev < 0.5)
		{
			if (temp < 0)
			{
				if (precp > 30)
					return BiomeEnum::Snow;
				return BiomeEnum::Ice;
			}
			return BiomeEnum::Beach;
		}
		if (temp > 20)
		{
			if (precp < 40)
				return BiomeEnum::Desert;
			if (precp < 130)
				return BiomeEnum::Savanna;
			if (precp < 230)
				return BiomeEnum::TropicalSeasonalForest;
			return BiomeEnum::TropicalRainForest;
		}
		if (temp > 5)
		{
			if (precp < 30)
				return BiomeEnum::Shrubland;
			if (precp < 100)
				return BiomeEnum::Grassland;
			if (precp < 200)
				return BiomeEnum::TemperateSeasonalForest;
			return BiomeEnum::TemperateRainForest;
		}
		if (temp > -5)
		{
			if (precp < 30)
				return BiomeEnum::Bare;
			if (precp < 150)
				return BiomeEnum::Taiga;
			return BiomeEnum::Snow;
		}
		if (temp > -15)
		{
			if (precp < 10)
				return BiomeEnum::Bare;
			if (precp < 30)
				return BiomeEnum::Tundra;
			return BiomeEnum::Snow;
		}
		if (precp > 30)
			return BiomeEnum::Snow;
		return BiomeEnum::Bare;
	}

	vec3 biomeColor(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean: return vec3(54, 54, 97) / 255;
		case BiomeEnum::Ice: return vec3(61, 81, 82) / 255;
		case BiomeEnum::Beach: return vec3(172, 159, 139) / 255;
		case BiomeEnum::Slope: return vec3(0) / 255;
		case BiomeEnum::Snow: return vec3(248) / 255;
		case BiomeEnum::Bare: return vec3(187) / 255;
		case BiomeEnum::Tundra: return vec3(221, 221, 187) / 255;
		case BiomeEnum::Taiga: return vec3(204, 212, 187) / 255;
		case BiomeEnum::Shrubland: return vec3(196, 204, 187) / 255;
		case BiomeEnum::Grassland: return vec3(196, 212, 170) / 255;
		case BiomeEnum::TemperateSeasonalForest: return vec3(180, 201, 169) / 255;
		case BiomeEnum::TemperateRainForest: return vec3(164, 196, 168) / 255;
		case BiomeEnum::Desert: return vec3(233, 221, 199) / 255;
		case BiomeEnum::Savanna: return vec3(228, 232, 202) / 255;
		case BiomeEnum::TropicalSeasonalForest: return vec3(169, 204, 164) / 255;
		case BiomeEnum::TropicalRainForest: return vec3(156, 187, 169) / 255;
		default: return vec3::Nan();
		};
	}

	real biomeRoughness(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean: return 0.3;
		case BiomeEnum::Ice: return 0.15;
		case BiomeEnum::Beach: return 0.7;
		case BiomeEnum::Slope: return 0.8;
		case BiomeEnum::Snow: return 0.7;
		case BiomeEnum::Bare: return 0.9;
		case BiomeEnum::Tundra: return 0.7;
		case BiomeEnum::Taiga: return 0.7;
		case BiomeEnum::Shrubland: return 0.8;
		case BiomeEnum::Grassland: return 0.7;
		case BiomeEnum::TemperateSeasonalForest: return 0.7;
		case BiomeEnum::TemperateRainForest: return 0.7;
		case BiomeEnum::Desert: return 0.7;
		case BiomeEnum::Savanna: return 0.7;
		case BiomeEnum::TropicalSeasonalForest: return 0.7;
		case BiomeEnum::TropicalRainForest: return 0.7;
		default: return real::Nan();
		};
	}

	real biomeHeight(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean: return 0.1;
		case BiomeEnum::Ice: return 0.1;
		case BiomeEnum::Beach: return 0.3;
		case BiomeEnum::Slope: return 0.5;
		case BiomeEnum::Snow: return 0.9;
		case BiomeEnum::Bare: return 0.3;
		case BiomeEnum::Tundra: return 0.4;
		case BiomeEnum::Taiga: return 0.6;
		case BiomeEnum::Shrubland: return 0.4;
		case BiomeEnum::Grassland: return 0.6;
		case BiomeEnum::TemperateSeasonalForest: return 0.7;
		case BiomeEnum::TemperateRainForest: return 0.8;
		case BiomeEnum::Desert: return 0.3;
		case BiomeEnum::Savanna: return 0.4;
		case BiomeEnum::TropicalSeasonalForest: return 0.7;
		case BiomeEnum::TropicalRainForest: return 0.8;
		default: return real::Nan();
		};
	}

	real biomeDiversity(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean: return 0.03;
		case BiomeEnum::Ice: return 0.03;
		case BiomeEnum::Beach: return 0.05;
		case BiomeEnum::Slope: return 0.07;
		case BiomeEnum::Snow: return 0.02;
		case BiomeEnum::Bare: return 0.04;
		case BiomeEnum::Tundra: return 0.03;
		case BiomeEnum::Taiga: return 0.03;
		case BiomeEnum::Shrubland: return 0.04;
		case BiomeEnum::Grassland: return 0.06;
		case BiomeEnum::TemperateSeasonalForest: return 0.07;
		case BiomeEnum::TemperateRainForest: return 0.09;
		case BiomeEnum::Desert: return 0.04;
		case BiomeEnum::Savanna: return 0.06;
		case BiomeEnum::TropicalSeasonalForest: return 0.07;
		case BiomeEnum::TropicalRainForest: return 0.09;
		default: return real::Nan();
		};
	}

	uint8 biomeTerrainType(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Ocean:
			return 8; // deep water
		case BiomeEnum::Beach:
			return 7; // shallow water
		case BiomeEnum::Slope:
			return 4; // slope
		case BiomeEnum::Shrubland:
		case BiomeEnum::Grassland:
		case BiomeEnum::Savanna:
		case BiomeEnum::TemperateSeasonalForest:
		case BiomeEnum::TemperateRainForest:
		case BiomeEnum::TropicalSeasonalForest:
		case BiomeEnum::TropicalRainForest:
			return 5; // fast
		case BiomeEnum::Ice:
		case BiomeEnum::Snow:
		case BiomeEnum::Tundra:
		case BiomeEnum::Taiga:
		case BiomeEnum::Desert:
			return 2; // slow
		case BiomeEnum::Bare:
		default:
			return 0; // road
		}
	}

	//---------
	// terrain
	//---------

	real terrainElevation(const vec3 &pos)
	{
		static const Holder<NoiseFunction> elevNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		return elevNoise->evaluate(pos * 0.02) * 15 + 0;
		/*
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
		return pow(clouds1->evaluate(p1 * 5) * 0.5 + 0.5, 1.8) * pow(clouds2->evaluate(p2 * 8) * 0.5 + 0.5, 1.5) * 15 - 0.6;
		*/
	}

	real terrainPrecipitation(const vec3 &pos)
	{
		static const Holder<NoiseFunction> precpNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = precpNoise->evaluate(pos * 0.02) * 0.5 + 0.5;
		p = pow(p, 2.5);
		return p * 400;
	}

	real terrainTemperature(const vec3 &pos, real elev)
	{
		static const Holder<NoiseFunction> tempNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		return 30 - max(elev, 0) * 2.2 + tempNoise->evaluate(pos * 0.01) * 10;
	}

	void terrainPoles(const vec3 &pos, real &temp)
	{
		static const Holder<NoiseFunction> polarNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real polar = abs(atan(pos[1] / length(vec2(pos[0], pos[2]))).value) / real::Pi() * 2;
		polar = pow(polar, 3);
		polar += polarNoise->evaluate(pos * 0.07) * 0.1;
		temp -= polar * 60;
	}

	vec3 anyPerpendicular(const vec3 &a)
	{
		CAGE_ASSERT(abs(length(a) - 1) < 1e-4);
		vec3 b = vec3(1, 0, 0);
		if (abs(dot(a, b)) > 0.9)
			b = vec3(0, 1, 0);
		return normalize(cross(a, b));
	}

	rads terrainSlope(const vec3 &pos, const vec3 &normal, real radius = 0.05)
	{
		vec3 a = anyPerpendicular(normal);
		vec3 b = cross(normal, a);
		a *= radius;
		b *= radius;
		vec3 c = (a + b) / sqrt(2);
		vec3 d = (a - b) / sqrt(2);
		real elevs[8] = {
			terrainElevation(pos + a),
			terrainElevation(pos + b),
			terrainElevation(pos - a),
			terrainElevation(pos - b),
			terrainElevation(pos + c),
			terrainElevation(pos + d),
			terrainElevation(pos - c),
			terrainElevation(pos - d),
		};
		real difs[4] = {
			abs(elevs[2] - elevs[0]),
			abs(elevs[3] - elevs[1]),
			abs(elevs[6] - elevs[4]),
			abs(elevs[7] - elevs[5]),
		};
		real md = max(max(difs[0], difs[1]), max(difs[2], difs[3]));
		return atan(md / radius);
	}

	void oceanOverrides(const vec3 &pos, BiomeEnum biom, real elev, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const vec3 shallowColor = vec3(26, 102, 125) / 255;
		if (biom != BiomeEnum::Ocean)
			return;
		real shallow = -0.5 / (elev - 0.5);
		albedo = interpolate(albedo, shallowColor, shallow);
		if (shallow > 0.5)
			terrainType = 7;
		// todo waves
	}

	void iceOverrides(const vec3 &pos, BiomeEnum biom, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Natural;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> scaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (biom != BiomeEnum::Ice)
			return;
		real scale = 0.5 + scaleNoise->evaluate(pos * 0.3) * 0.02;
		real crack = cracksNoise->evaluate(pos * scale);
		crack = pow(crack, 0.3);
		height += (crack - 0.5) * 0.2;
		albedo += crack * 0.3;
		special[0] += (1 - crack) * 0.6;
	}

	void slopeOverrides(const vec3 &pos, BiomeEnum biom, vec3 &albedo, vec2 &special, real &height)
	{
		static const uint32 seed = noiseSeed();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Natural;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> typeNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Natural;
			cfg.operation = NoiseOperationEnum::NoiseLookup;
			cfg.seed = seed;
			return newNoiseFunction(cfg, []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 2;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}());
		}();
		if (biom != BiomeEnum::Slope)
			return;
		real scale = 3;
		real crack = cracksNoise->evaluate(pos * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.8);
		real type = typeNoise->evaluate(pos * scale) * 0.5 + 0.5;
		albedo = vec3((type * 0.6 + 0.2) * crack);
		special[0] = interpolate(0.9, 0.3 + type * 0.6, crack);
		height = 0.35 + crack * 0.3;
	}

	void terrain(const vec3 &pos, const vec3 &normal, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		real elev = terrainElevation(pos);
		rads slope = terrainSlope(pos, normal);
		real temp = terrainTemperature(pos, elev);
		terrainPoles(pos, temp);
		real precp = terrainPrecipitation(pos);
		BiomeEnum biom = biome(elev, slope, temp, precp);
		terrainType = biomeTerrainType(biom);
		real diversity = biomeDiversity(biom);
		albedo = biomeColor(biom);
		special = vec2(biomeRoughness(biom), 0);
		height = biomeHeight(biom);
		oceanOverrides(pos, biom, elev, terrainType, albedo, special, height);
		iceOverrides(pos, biom, albedo, special, height);
		slopeOverrides(pos, biom, albedo, special, height);
		albedo = colorDeviation(albedo, diversity);
		special[0] = special[0] + (randomChance() - 0.5) * diversity * 2;
		albedo = clamp(albedo, 0, 1);
		special = clamp(special, 0, 1);
		height = clamp(height, 0, 1);
	}
}

real functionDensity(const vec3 &pos)
{
	return baseShapeDensity(pos) + max(terrainElevation(pos), 0);
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
}

void printTerrainTypeStatistics(const std::vector<uint8> &types)
{
	uint32 counts[256];
	for (uint32 &c : counts)
		c = 0;
	for (uint8 t : types)
		counts[t]++;
	CAGE_LOG(SeverityEnum::Info, "terrainTypes", "terrain types:");
	for (uint32 i = 0; i < 256; i++)
	{
		if (counts[i])
		{
			CAGE_LOG_CONTINUE(SeverityEnum::Info, "terrainTypes", stringizer() + i + ": " + counts[i]);
		}
	}
}

