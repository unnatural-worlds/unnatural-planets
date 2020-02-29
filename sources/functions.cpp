#include "generator.h"

#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>

namespace
{
	uint32 globalSeed = (uint32)detail::getApplicationRandomGenerator().next();

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

	vec3 anyPerpendicular(const vec3 &a)
	{
		CAGE_ASSERT(abs(length(a) - 1) < 1e-4);
		vec3 b = vec3(1, 0, 0);
		if (abs(dot(a, b)) > 0.9)
			b = vec3(0, 1, 0);
		return normalize(cross(a, b));
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
		const real margin = 5;
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
			return r + margin;
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
			return -r + margin;
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

	class TerrainType
	{
	public:
		enum Types
		{
			Road = 0,
			Slow = 2,
			Fast = 5,
			SteepSlope = 4,
			ShallowWater = 7,
			DeepWater = 8,
		};
	};

	BiomeEnum biome(real elev, rads slope, real temp, real precp)
	{
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
			return BiomeEnum::Taiga;
		}
		if (precp < 10)
			return BiomeEnum::Bare;
		return BiomeEnum::Tundra;
	}

	vec3 biomeColor(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Bare: return vec3(187) / 255;
		case BiomeEnum::Tundra: return vec3(221, 221, 187) / 255;
		case BiomeEnum::Taiga: return vec3(204, 212, 187) / 255;
		case BiomeEnum::Shrubland: return vec3(196, 204, 187) / 255;
		case BiomeEnum::Grassland: return vec3(196, 212, 170) / 255;
		case BiomeEnum::TemperateSeasonalForest: return vec3(180, 201, 169) / 255;
		case BiomeEnum::TemperateRainForest: return vec3(164, 196, 168) / 255;
		case BiomeEnum::Desert: return vec3(172, 159, 139) / 255;
		case BiomeEnum::Savanna: return vec3(228, 232, 202) / 255;
		case BiomeEnum::TropicalSeasonalForest: return vec3(169, 204, 164) / 255;
		case BiomeEnum::TropicalRainForest: return vec3(156, 187, 169) / 255;
		default: return vec3::Nan();
		};
	}

	static const vec3 deepWaterColor = vec3(54, 54, 97) / 255;
	static const vec3 shallowWaterColor = vec3(26, 102, 125) / 255;

	real biomeRoughness(BiomeEnum b)
	{
		switch (b)
		{
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
		case BiomeEnum::Bare: return 0.2;
		case BiomeEnum::Tundra: return 0.8;
		case BiomeEnum::Taiga: return 0.7;
		case BiomeEnum::Shrubland: return 0.6;
		case BiomeEnum::Grassland: return 0.5;
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
		case BiomeEnum::Shrubland:
		case BiomeEnum::Grassland:
		case BiomeEnum::Savanna:
		case BiomeEnum::TemperateSeasonalForest:
		case BiomeEnum::TropicalSeasonalForest:
			return TerrainType::Fast;
		case BiomeEnum::Tundra:
		case BiomeEnum::Taiga:
		case BiomeEnum::Desert:
		case BiomeEnum::TemperateRainForest:
		case BiomeEnum::TropicalRainForest:
			return TerrainType::Slow;
		case BiomeEnum::Bare:
		default:
			return TerrainType::Road;
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
		return 40 - abs(elev) * 2.2 + tempNoise->evaluate(pos * 0.01) * 10;
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
		polar = pow(polar, 1.2);
		polar += polarNoise->evaluate(pos * 0.07) * 0.1;
		temp -= polar * 80;
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

	void oceanOverrides(const vec3 &pos, real elev, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		if (elev > 0)
			return;
		CAGE_ASSERT(elev <= 0);
		real shallow = -0.5 / (elev - 0.5);
		terrainType = shallow > 0.5 ? TerrainType::ShallowWater : TerrainType::DeepWater;
		albedo = interpolate(deepWaterColor, shallowWaterColor, shallow);
		special[0] = 0.3;
		height = 0;
		// todo waves
	}

	void iceOverrides(const vec3 &pos, real elev, real temp, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
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
		if (elev > -2 || temp > -5)
			return;
		real scale = 0.5 + scaleNoise->evaluate(pos * 0.3) * 0.02;
		real crack = cracksNoise->evaluate(pos * scale);
		crack = pow(crack, 0.3);
		terrainType = TerrainType::Slow;
		albedo = vec3(61, 81, 82) / 255 + crack * 0.3;
		special[0] = 0.15 + (1 - crack) * 0.6;
		height = 0.2 + crack * 0.2;
	}

	void slopeOverrides(const vec3 &pos, real elev, rads slope, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
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
		if (elev < 0)
			return;
		real factor = rescale(slope.value, 0, real::Pi() * 0.5, -1, 2);
		factor = sharpEdge(factor);
		real scale = 3;
		real crack = cracksNoise->evaluate(pos * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.8);
		real type = typeNoise->evaluate(pos * scale) * 0.5 + 0.5;
		if (factor > 0.5)
			terrainType = TerrainType::SteepSlope;
		vec3 rocksAlbedo = vec3((type * 0.6 + 0.2) * crack);
		albedo = interpolate(albedo, rocksAlbedo, factor);
		real rocksRoughness = interpolate(0.9, 0.3 + type * 0.6, crack);
		special[0] = interpolate(special[0], rocksRoughness, factor);
		real rocksHeight = 0.35 + crack * 0.3;
		height = interpolate(height, rocksHeight, factor);
	}

	void grassOverrides(const vec3 &pos, real elev, rads slope, real temp, real precp, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> threadsNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::RigidMulti;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> perturbNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (elev < 0)
			return;
		real thriving = 0.9;
		thriving *= clamp(rescale(slope.value, 0, real::Pi() / 2, 1.5, -0.5), 0, 1);
		thriving *= 2000 / (pow(abs(temp - 20), 4) + 2000);
		thriving *= pow(precp, 0.5) / (abs(precp - 70) + 10);
		real threshold = thresholdNoise->evaluate(pos * 1.5) * 0.5 + 0.5;
		real factor = sharpEdge(thriving - threshold + 0.5, 0.15);
		real blend = threadsNoise->evaluate(pos * (2 + perturbNoise->evaluate(pos) * 0.1)) * 0.5 + 0.5;
		vec3 grassAlbedo = interpolate(vec3(128, 152, 74), vec3(72, 106, 39), blend) / 255;
		albedo = interpolate(albedo, grassAlbedo, factor);
		special[0] = interpolate(special[0], 0.5, factor);
		height += (0.03 + blend * 0.05) * factor;
	}

	void snowOverrides(const vec3 &pos, real elev, rads slope, real temp, real precp, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if ((elev < 0 || temp > 0) && (elev > -2 || temp > -5))
			return;
		if (precp < 50 || terrainType == TerrainType::SteepSlope)
			return;
		real factor = (thresholdNoise->evaluate(pos) * 0.5 + 0.5) * 0.6 + 0.4;
		terrainType = TerrainType::Slow;
		albedo = interpolate(albedo, vec3(248) / 255, factor);
		special[0] = interpolate(special[0], 0.7, factor);
		height += 0.3 * factor;
	}

	void beachOverrides(const vec3 &pos, real elev, uint8 &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> solidNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (elev < 0 || elev > 0.5)
			return;
		real solid = elev / 0.5;
		solid += solidNoise->evaluate(pos * 1.5) * 0.3;
		solid = clamp(solid, 0, 1);
		terrainType = TerrainType::ShallowWater;
		albedo = interpolate(shallowWaterColor, albedo, solid);
		special[0] = interpolate(0.3, special[0], solid);
		height = solid * 0.2;
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
		oceanOverrides(pos, elev, terrainType, albedo, special, height);
		iceOverrides(pos, elev, temp, terrainType, albedo, special, height);
		slopeOverrides(pos, elev, slope, terrainType, albedo, special, height);
		grassOverrides(pos, elev, slope, temp, precp, albedo, special, height);
		snowOverrides(pos, elev, slope, temp, precp, terrainType, albedo, special, height);
		beachOverrides(pos, elev, terrainType, albedo, special, height);
		albedo = colorDeviation(albedo, diversity);
		special[0] = special[0] + (randomChance() - 0.5) * diversity * 2;
		static const real mm = 1.01 / 255; // prevent valid values from being overridden by texture inpaint
		albedo = clamp(albedo, mm, 1);
		special = clamp(special, mm, 1);
		height = clamp(height, mm, 1);
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

