#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>
#include <cage-core/config.h>

#include "terrain.h"
#include "generator.h"
#include "math.h"

namespace
{
	ConfigBool useTerrainPoles("unnatural-planets/planet/poles");

	constexpr real globalPositionScale = 0.1;

	//--------
	// biomes
	//--------

	TerrainBiomeEnum terrainBiome(real elevation, rads slope, real temperature, real precipitation)
	{
		if (temperature > 20)
		{
			if (precipitation < 40)
				return TerrainBiomeEnum::Desert;
			if (precipitation < 130)
				return TerrainBiomeEnum::Savanna;
			if (precipitation < 230)
				return TerrainBiomeEnum::TropicalSeasonalForest;
			return TerrainBiomeEnum::TropicalRainForest;
		}
		if (temperature > 5)
		{
			if (precipitation < 30)
				return TerrainBiomeEnum::Shrubland;
			if (precipitation < 100)
				return TerrainBiomeEnum::Grassland;
			if (precipitation < 200)
				return TerrainBiomeEnum::TemperateSeasonalForest;
			return TerrainBiomeEnum::TemperateRainForest;
		}
		if (temperature > -5)
		{
			if (precipitation < 30)
				return TerrainBiomeEnum::Bare;
			return TerrainBiomeEnum::Taiga;
		}
		if (precipitation < 10)
			return TerrainBiomeEnum::Bare;
		return TerrainBiomeEnum::Tundra;
	}

	vec3 biomeColor(TerrainBiomeEnum b)
	{
		switch (b)
		{
		case TerrainBiomeEnum::Bare: return vec3(187) / 255;
		case TerrainBiomeEnum::Tundra: return vec3(221, 221, 187) / 255;
		case TerrainBiomeEnum::Taiga: return vec3(204, 212, 187) / 255;
		case TerrainBiomeEnum::Shrubland: return vec3(196, 204, 187) / 255;
		case TerrainBiomeEnum::Grassland: return vec3(196, 212, 170) / 255;
		case TerrainBiomeEnum::TemperateSeasonalForest: return vec3(180, 201, 169) / 255;
		case TerrainBiomeEnum::TemperateRainForest: return vec3(164, 196, 168) / 255;
		case TerrainBiomeEnum::Desert: return vec3(172, 159, 139) / 255;
		case TerrainBiomeEnum::Savanna: return vec3(228, 232, 202) / 255;
		case TerrainBiomeEnum::TropicalSeasonalForest: return vec3(169, 204, 164) / 255;
		case TerrainBiomeEnum::TropicalRainForest: return vec3(156, 187, 169) / 255;
		default: return vec3::Nan();
		};
	}

	constexpr vec3 deepWaterColor = vec3(54, 54, 97) / 255;
	constexpr vec3 shallowWaterColor = vec3(26, 102, 125) / 255;

	real biomeRoughness(TerrainBiomeEnum b)
	{
		switch (b)
		{
		case TerrainBiomeEnum::Bare: return 0.9;
		case TerrainBiomeEnum::Tundra: return 0.7;
		case TerrainBiomeEnum::Taiga: return 0.7;
		case TerrainBiomeEnum::Shrubland: return 0.8;
		case TerrainBiomeEnum::Grassland: return 0.7;
		case TerrainBiomeEnum::TemperateSeasonalForest: return 0.7;
		case TerrainBiomeEnum::TemperateRainForest: return 0.7;
		case TerrainBiomeEnum::Desert: return 0.7;
		case TerrainBiomeEnum::Savanna: return 0.7;
		case TerrainBiomeEnum::TropicalSeasonalForest: return 0.7;
		case TerrainBiomeEnum::TropicalRainForest: return 0.7;
		default: return real::Nan();
		};
	}

	real biomeHeight(TerrainBiomeEnum b)
	{
		switch (b)
		{
		case TerrainBiomeEnum::Bare: return 0.2;
		case TerrainBiomeEnum::Tundra: return 0.8;
		case TerrainBiomeEnum::Taiga: return 0.7;
		case TerrainBiomeEnum::Shrubland: return 0.6;
		case TerrainBiomeEnum::Grassland: return 0.5;
		case TerrainBiomeEnum::TemperateSeasonalForest: return 0.7;
		case TerrainBiomeEnum::TemperateRainForest: return 0.8;
		case TerrainBiomeEnum::Desert: return 0.3;
		case TerrainBiomeEnum::Savanna: return 0.4;
		case TerrainBiomeEnum::TropicalSeasonalForest: return 0.7;
		case TerrainBiomeEnum::TropicalRainForest: return 0.8;
		default: return real::Nan();
		};
	}

	real biomeDiversity(TerrainBiomeEnum b)
	{
		switch (b)
		{
		case TerrainBiomeEnum::Bare: return 0.04;
		case TerrainBiomeEnum::Tundra: return 0.03;
		case TerrainBiomeEnum::Taiga: return 0.03;
		case TerrainBiomeEnum::Shrubland: return 0.04;
		case TerrainBiomeEnum::Grassland: return 0.06;
		case TerrainBiomeEnum::TemperateSeasonalForest: return 0.07;
		case TerrainBiomeEnum::TemperateRainForest: return 0.09;
		case TerrainBiomeEnum::Desert: return 0.04;
		case TerrainBiomeEnum::Savanna: return 0.06;
		case TerrainBiomeEnum::TropicalSeasonalForest: return 0.07;
		case TerrainBiomeEnum::TropicalRainForest: return 0.09;
		default: return real::Nan();
		};
	}

	TerrainTypeEnum biomeTerrainType(TerrainBiomeEnum b)
	{
		switch (b)
		{
		case TerrainBiomeEnum::Shrubland:
		case TerrainBiomeEnum::Grassland:
		case TerrainBiomeEnum::Savanna:
		case TerrainBiomeEnum::TemperateSeasonalForest:
		case TerrainBiomeEnum::TropicalSeasonalForest:
			return TerrainTypeEnum::Fast;
		case TerrainBiomeEnum::Tundra:
		case TerrainBiomeEnum::Taiga:
		case TerrainBiomeEnum::Desert:
		case TerrainBiomeEnum::TemperateRainForest:
		case TerrainBiomeEnum::TropicalRainForest:
			return TerrainTypeEnum::Slow;
		case TerrainBiomeEnum::Bare:
		default:
			return TerrainTypeEnum::Road;
		}
	}

	//---------
	// terrain
	//---------

	void terrainPrecipitation(Tile &tile)
	{
		static const Holder<NoiseFunction> precpNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = precpNoise->evaluate(tile.position * 0.015) * 0.5 + 0.5;
		p = clamp(p, 0, 1);
		p = smootherstep(p);
		p = smootherstep(p);
		p = smootherstep(p);
		p = pow(p, 1.6);
		p += max(1 - abs(tile.elevation), 0) * 0.15; // more water close to oceans
		tile.precipitation = p * 400;
	}

	void terrainTemperature(Tile &tile)
	{
		static const Holder<NoiseFunction> tempNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real t = tempNoise->evaluate(tile.position * 0.015) * 0.5 + 0.5;
		t = clamp(t, 0, 1);
		t = smootherstep(t);
		t = smootherstep(t);
		t = t * 2 - 1;
		tile.temperature = 30 + t * 5 - abs(tile.elevation) * 2.8;
	}

	void terrainPoles(Tile &tile)
	{
		static const Holder<NoiseFunction> polarNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (!useTerrainPoles)
			return;
		real polar = abs(atan(tile.position[1] / length(vec2(tile.position[0], tile.position[2]))).value) / real::Pi() * 2;
		polar = pow(polar, 1.7);
		polar += polarNoise->evaluate(tile.position * 0.07) * 0.1;
		tile.temperature += 15 - polar * 80;
	}

	void terrainSlope(Tile &tile, real radius = 0.05)
	{
		vec3 a = anyPerpendicular(tile.normal);
		vec3 b = cross(tile.normal, a);
		a *= radius;
		b *= radius;
		vec3 c = (a + b) / sqrt(2);
		vec3 d = (a - b) / sqrt(2);
		real elevs[8] = {
			terrainElevation((tile.position + a) / globalPositionScale),
			terrainElevation((tile.position + b) / globalPositionScale),
			terrainElevation((tile.position - a) / globalPositionScale),
			terrainElevation((tile.position - b) / globalPositionScale),
			terrainElevation((tile.position + c) / globalPositionScale),
			terrainElevation((tile.position + d) / globalPositionScale),
			terrainElevation((tile.position - c) / globalPositionScale),
			terrainElevation((tile.position - d) / globalPositionScale),
		};
		real difs[4] = {
			abs(elevs[2] - elevs[0]),
			abs(elevs[3] - elevs[1]),
			abs(elevs[6] - elevs[4]),
			abs(elevs[7] - elevs[5]),
		};
		real md = max(max(difs[0], difs[1]), max(difs[2], difs[3]));
		tile.slope = atan(md / radius);
	}

	void oceanOverrides(Tile &tile)
	{
		if (tile.elevation > 0)
			return;
		CAGE_ASSERT(tile.elevation <= 0);
		real shallow = -0.5 / (tile.elevation - 0.5);
		shallow = clamp(shallow, 0, 1);
		shallow = smoothstep(shallow);
		tile.type = shallow > 0.5 ? TerrainTypeEnum::ShallowWater : TerrainTypeEnum::DeepWater;
		tile.albedo = interpolate(deepWaterColor, shallowWaterColor, shallow);
		tile.special[0] = 0.3;
		tile.biome = TerrainBiomeEnum::_Ocean;

		{ // waves
			static const Holder<NoiseFunction> xNoise = []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> yNoise = []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> zNoise = []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			vec3 s = tile.position * 0.01;
			real x = xNoise->evaluate(s);
			real y = yNoise->evaluate(s);
			real z = zNoise->evaluate(s);
			vec3 dir = normalize(vec3(x, y, z));
			//CAGE_ASSERT(isUnit(dir));
			//dir = normalize(dir - dot(dir, normal) * normal);
			CAGE_ASSERT(isUnit(dir));
			real dist = dot(dir, tile.position) * length(tile.position);
			real wave = sin(rads(dist * 0.05));
			tile.height = wave * 0.05 + 0.05;
		}
	}

	void iceOverrides(Tile &tile)
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
		if (tile.elevation > -2 || tile.temperature > -5)
			return;
		real scale = 0.5 + scaleNoise->evaluate(tile.position * 0.3) * 0.02;
		real crack = cracksNoise->evaluate(tile.position * scale);
		crack = pow(crack, 0.3);
		tile.type = TerrainTypeEnum::Slow;
		tile.albedo = vec3(61, 81, 82) / 255 + crack * 0.3;
		tile.special[0] = 0.15 + (1 - crack) * 0.6;
		tile.height = 0.2 + crack * 0.2;
	}

	void slopeOverrides(Tile &tile)
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
		if (tile.elevation < 0)
			return;
		real factor = rescale(tile.slope.value, 0, real::Pi() * 0.5, -1, 2);
		factor = sharpEdge(factor);
		real scale = 3;
		real crack = cracksNoise->evaluate(tile.position * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.8);
		real type = typeNoise->evaluate(tile.position * scale) * 0.5 + 0.5;
		if (factor > 0.5)
			tile.type = TerrainTypeEnum::SteepSlope;
		vec3 rocksAlbedo = vec3((type * 0.6 + 0.2) * crack);
		tile.albedo = interpolate(tile.albedo, rocksAlbedo, factor);
		real rocksRoughness = interpolate(0.9, 0.3 + type * 0.6, crack);
		tile.special[0] = interpolate(tile.special[0], rocksRoughness, factor);
		real rocksHeight = 0.35 + crack * 0.3;
		tile.height = interpolate(tile.height, rocksHeight, factor);
	}

	void grassOverrides(Tile &tile)
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
		if (tile.elevation < 0)
			return;
		real thriving = 0.9;
		thriving *= clamp(rescale(tile.slope.value, 0, real::Pi() / 2, 1.5, -0.5), 0, 1);
		thriving *= 2000 / (pow(abs(tile.temperature - 20), 4) + 2000);
		thriving *= pow(tile.precipitation, 0.5) / (abs(tile.precipitation - 70) + 10);
		real threshold = thresholdNoise->evaluate(tile.position * 1.5) * 0.5 + 0.5;
		real factor = sharpEdge(thriving - threshold + 0.5, 0.15);
		real blend = threadsNoise->evaluate(tile.position * (2 + perturbNoise->evaluate(tile.position) * 0.1)) * 0.5 + 0.5;
		vec3 grassAlbedo = interpolate(vec3(128, 152, 74), vec3(72, 106, 39), blend) / 255;
		tile.albedo = interpolate(tile.albedo, grassAlbedo, factor);
		tile.special[0] = interpolate(tile.special[0], 0.5, factor);
		tile.height += (0.03 + blend * 0.05) * factor;
	}

	void snowOverrides(Tile &tile)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if ((tile.elevation < 0 || tile.temperature > 0) && (tile.elevation > -2 || tile.temperature > -5))
			return;
		if (tile.precipitation < 50 || tile.type == TerrainTypeEnum::SteepSlope)
			return;
		real factor = (thresholdNoise->evaluate(tile.position) * 0.5 + 0.5) * 0.6 + 0.4;
		tile.type = TerrainTypeEnum::Slow;
		tile.albedo = interpolate(tile.albedo, vec3(248) / 255, factor);
		tile.special[0] = interpolate(tile.special[0], 0.7, factor);
		tile.height += 0.3 * factor;
	}

	void beachOverrides(Tile &tile)
	{
		static const Holder<NoiseFunction> solidNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (tile.elevation < 0 || tile.elevation > 0.3)
			return;
		real solid = tile.elevation / 0.3;
		solid += solidNoise->evaluate(tile.position * 1.5) * 0.3;
		solid = clamp(solid, 0, 1);
		tile.type = TerrainTypeEnum::ShallowWater;
		tile.albedo = interpolate(shallowWaterColor, tile.albedo, solid);
		tile.special[0] = interpolate(0.3, tile.special[0], solid);
		tile.height = solid * 0.2;
	}

	void terrainFertility(Tile &tile)
	{
		static const Holder<NoiseFunction> fertNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = fertNoise->evaluate(tile.position * 0.035) * 2;
		p = clamp(p, -1, 1);
		tile.fertility = p;
	}

	void terrainNationality(Tile &tile)
	{
		static const Holder<NoiseFunction> natNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = natNoise->evaluate(tile.position * 0.015) * 5;
		p = clamp(p, -1, 1);
		tile.nationality = p;
	}
}

void terrainTile(Tile &tile)
{
	tile.position *= globalPositionScale;
	CAGE_ASSERT(isUnit(tile.normal));
	tile.elevation = terrainElevation(tile.position / globalPositionScale);
	terrainSlope(tile);
	terrainTemperature(tile);
	terrainPoles(tile);
	terrainPrecipitation(tile);
	tile.biome = terrainBiome(tile.elevation, tile.slope, tile.temperature, tile.precipitation);
	tile.type = biomeTerrainType(tile.biome);
	real diversity = biomeDiversity(tile.biome);
	tile.albedo = biomeColor(tile.biome);
	tile.special[0] = biomeRoughness(tile.biome);
	tile.height = biomeHeight(tile.biome);
	oceanOverrides(tile);
	iceOverrides(tile);
	slopeOverrides(tile);
	grassOverrides(tile);
	snowOverrides(tile);
	beachOverrides(tile);
	tile.albedo = saturate(tile.albedo);
	tile.albedo = colorDeviation(tile.albedo, diversity);
	tile.special[0] = tile.special[0] + (randomChance() - 0.5) * diversity * 2;
	tile.albedo = saturate(tile.albedo);
	tile.special = saturate(tile.special);
	tile.height = saturate(tile.height);
	terrainNationality(tile);
	terrainFertility(tile);
	tile.position /= globalPositionScale;
}

void terrainPreseed()
{
	terrainDensity(vec3());
	Tile tile;
	tile.normal = vec3(0, 1, 0);
	terrainTile(tile);
}
