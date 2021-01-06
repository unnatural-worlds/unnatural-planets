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

	void generateElevation(Tile &tile)
	{
		static const Holder<NoiseFunction> elevNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> maskNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = elevNoise->evaluate(tile.position * 0.1);
		real m = maskNoise->evaluate(tile.position * 0.005);
		m = 1 - smootherstep(abs(m));
		tile.elevation += p * m * 0.3;
	}

	void generateSlope(Tile &tile)
	{
		constexpr real radius = 0.5;
		vec3 a = anyPerpendicular(tile.normal);
		vec3 b = cross(tile.normal, a);
		a *= radius;
		b *= radius;
		vec3 c = (a + b) / sqrt(2);
		vec3 d = (a - b) / sqrt(2);
		real elevs[8] = {
			terrainElevation(tile.position + a),
			terrainElevation(tile.position + b),
			terrainElevation(tile.position - a),
			terrainElevation(tile.position - b),
			terrainElevation(tile.position + c),
			terrainElevation(tile.position + d),
			terrainElevation(tile.position - c),
			terrainElevation(tile.position - d),
		};
		real difs[4] = {
			abs(elevs[2] - elevs[0]),
			abs(elevs[3] - elevs[1]),
			abs(elevs[6] - elevs[4]),
			abs(elevs[7] - elevs[5]),
		};
		real md = max(max(difs[0], difs[1]), max(difs[2], difs[3]));
		tile.slope = atan(10 * md / radius);
	}

	void generateNationality(Tile &tile)
	{
		static const Holder<NoiseFunction> natNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = natNoise->evaluate(tile.position * 0.0015) * 5;
		p = clamp(p, -1, 1);
		tile.nationality = p;
	}

	void generatePrecipitation(Tile &tile)
	{
		static const Holder<NoiseFunction> precpNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = precpNoise->evaluate(tile.position * 0.0015) * 0.5 + 0.5;
		p = clamp(p, 0, 1);
		p = smootherstep(p);
		p = smootherstep(p);
		p = smootherstep(p);
		p = pow(p, 1.6);
		p += max(1 - abs(tile.elevation), 0) * 0.15; // more water close to oceans
		tile.precipitation = p * 400;
	}

	void generateTemperature(Tile &tile)
	{
		static const Holder<NoiseFunction> tempNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real t = tempNoise->evaluate(tile.position * 0.0015) * 0.5 + 0.5;
		t = clamp(t, 0, 1);
		t = smootherstep(t);
		t = smootherstep(t);
		t = t * 2 - 1;
		tile.temperature = 30 + t * 5 - abs(tile.elevation) * 2.8;
	}

	void generatePoles(Tile &tile)
	{
		static const Holder<NoiseFunction> polarNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (!useTerrainPoles)
			return;
		real polar = abs(atan(tile.position[1] / length(vec2(tile.position[0], tile.position[2]))).value) / real::Pi() * 2;
		polar = pow(polar, 1.7);
		polar += polarNoise->evaluate(tile.position * 0.007) * 0.1;
		tile.temperature += 15 - polar * 80;
	}

	void generateBiome(Tile &tile)
	{
		if (tile.temperature > 20)
		{
			if (tile.precipitation < 40)
				tile.biome = TerrainBiomeEnum::Desert;
			else if (tile.precipitation < 130)
				tile.biome = TerrainBiomeEnum::Savanna;
			else if (tile.precipitation < 230)
				tile.biome = TerrainBiomeEnum::TropicalSeasonalForest;
			else
				tile.biome = TerrainBiomeEnum::TropicalRainForest;
		}
		else if (tile.temperature > 5)
		{
			if (tile.precipitation < 30)
				tile.biome = TerrainBiomeEnum::Shrubland;
			else if (tile.precipitation < 100)
				tile.biome = TerrainBiomeEnum::Grassland;
			else if (tile.precipitation < 200)
				tile.biome = TerrainBiomeEnum::TemperateSeasonalForest;
			else
				tile.biome = TerrainBiomeEnum::TemperateRainForest;
		}
		else if (tile.temperature > -5)
		{
			if (tile.precipitation < 30)
				tile.biome = TerrainBiomeEnum::Bare;
			else
				tile.biome = TerrainBiomeEnum::Taiga;
		}
		else if (tile.precipitation < 10)
			tile.biome = TerrainBiomeEnum::Bare;
		else
			tile.biome = TerrainBiomeEnum::Tundra;
	}

	void generateType(Tile &tile)
	{
		if (tile.elevation < 0)
			tile.type = TerrainTypeEnum::ShallowWater;
		else if (tile.slope > degs(15))
			tile.type = TerrainTypeEnum::SteepSlope;
		else switch (tile.biome)
		{
		case TerrainBiomeEnum::Shrubland:
		case TerrainBiomeEnum::Grassland:
		case TerrainBiomeEnum::Savanna:
		case TerrainBiomeEnum::TemperateSeasonalForest:
		case TerrainBiomeEnum::TropicalSeasonalForest:
			tile.type = TerrainTypeEnum::Fast;
			break;
		case TerrainBiomeEnum::Tundra:
		case TerrainBiomeEnum::Taiga:
		case TerrainBiomeEnum::Desert:
		case TerrainBiomeEnum::TemperateRainForest:
		case TerrainBiomeEnum::TropicalRainForest:
			tile.type = TerrainTypeEnum::Slow;
			break;
		case TerrainBiomeEnum::Bare:
		default:
			tile.type = TerrainTypeEnum::Road;
			break;
		}
	}

	void generateBedrock(Tile &tile)
	{
		static const uint32 seed = noiseSeed();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> valueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Cell;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> saturationNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> scaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real scale = 0.3 + scaleNoise->evaluate(tile.position * 0.02) * 0.03;
		real crack = cracksNoise->evaluate(tile.position * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.8);
		real value = valueNoise->evaluate(tile.position * scale) * 0.5 + 0.5;
		real saturation = saturationNoise->evaluate(tile.position * 0.003) * 0.5 + 0.5;
		vec3 hsv = vec3(0.07, saturate(sharpEdge(saturation, 0.2)), (value * 0.6 + 0.2) * crack);
		tile.albedo = colorHsvToRgb(hsv);
		tile.roughness = interpolate(0.9, 0.3 + value * 0.6, crack);
		tile.height = 0.4 + crack * 0.4;
	}

	void generateWater(Tile &tile)
	{
		static const Holder<NoiseFunction> xNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> yNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> zNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (tile.elevation > 0)
			return;

		real bf = smoothstep(saturate(-tile.elevation - tile.height));
		real shallow = -0.5 / (tile.elevation - 0.5);
		shallow = saturate(shallow);
		shallow = smoothstep(shallow);
		real hueShift = hueNoise->evaluate(tile.position * 0.01) * 0.06;
		vec3 color = colorHueShift(interpolate(vec3(54, 54, 97), vec3(26, 102, 125), shallow) / 255, hueShift);
		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, 0.3, bf);
		tile.biome = TerrainBiomeEnum::Ocean;
		tile.type = shallow > 0.5 ? TerrainTypeEnum::ShallowWater : TerrainTypeEnum::DeepWater;

		{ // waves
			vec3 s = tile.position * 0.001;
			real x = xNoise->evaluate(s);
			real y = yNoise->evaluate(s);
			real z = zNoise->evaluate(s);
			vec3 dir = normalize(vec3(x, y, z));
			CAGE_ASSERT(isUnit(dir));
			real dist = dot(dir, tile.position) * length(tile.position);
			real wave = sin(rads(dist * 0.005)) * 0.05 + 0.8;
			tile.height = interpolate(tile.height, wave, bf);
		}
	}
}

void terrainTile(Tile &tile)
{
	CAGE_ASSERT(isUnit(tile.normal));
	tile.elevation = terrainElevation(tile.position);
	generateElevation(tile);
	generateSlope(tile);
	generateNationality(tile);
	generatePrecipitation(tile);
	generateTemperature(tile);
	generatePoles(tile);
	generateBiome(tile);
	generateType(tile);
	generateBedrock(tile);
	// mica (slída)
	// dirt
	// sand
	// corals
	// seaweed
	// boulders
	// tree stumps
	// small grass / moss / leaves
	generateWater(tile);
	// ice
	// large grass / flowers
	// snow
	tile.albedo = saturate(tile.albedo);
	tile.roughness = saturate(tile.roughness);
	tile.metallic = saturate(tile.metallic);
	tile.height = saturate(tile.height);
}

void terrainPreseed()
{
	terrainDensity(vec3());
	Tile tile;
	tile.normal = vec3(0, 1, 0);
	terrainTile(tile);
}
