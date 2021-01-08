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
		p = saturate(p);
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
		t = saturate(t);
		t = smootherstep(t);
		t = smootherstep(t);
		t = t * 2 - 1;
		tile.temperature = 30 + t * 5 - max(tile.elevation, 0) * 2.8;
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
		static const Holder<NoiseFunction> scaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> freqNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
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

		real scale = scaleNoise->evaluate(tile.position * 0.05) * 0.5 + 0.501;
		scale = sqr(scale) * 0.9;
		real freq = 0.25 + freqNoise->evaluate(tile.position * 0.009) * 0.05;
		real cracks = cracksNoise->evaluate(tile.position * freq) * 0.5 + 0.5;
		cracks = pow(cracks, 0.8);
		real value = valueNoise->evaluate(tile.position * freq) * 0.5 + 0.5;
		real saturation = saturationNoise->evaluate(tile.position * 0.003) * 0.5 + 0.5;
		vec3 hsv = vec3(0.07, saturate(sharpEdge(saturation, 0.2)), (value * 0.6 + 0.2) * cracks);
		tile.albedo = colorHsvToRgb(hsv);
		tile.roughness = interpolate(0.9, 0.3 + value * 0.6, cracks);
		tile.height = cracks * scale;
	}

	void generateSand(Tile &tile)
	{
		static const Holder<NoiseFunction> xNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> yNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> zNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
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

		real bf = saturate((tile.temperature - 17.5) * 0.2) * saturate((35 - tile.precipitation) * 0.1) * sharpEdge(saturate(1 - tile.slope.value * 1.5));
		if (bf < 1e-7)
			return;

		real height = 0;
		{ // waves
			vec3 s = tile.position * 0.0003;
			real x = xNoise->evaluate(s);
			real y = yNoise->evaluate(s);
			real z = zNoise->evaluate(s);
			vec3 dir = normalize(vec3(x, y, z));
			CAGE_ASSERT(isUnit(dir));
			real dist = dot(dir, normalize(tile.position)) * length(tile.position);
			height = sin(rads(dist * 0.001)) * 0.5 + 0.5;
			height = height * 0.4 + 0.3;
		}

		bf *= sharpEdge(saturate(height - tile.height + 0.4));

		real hueShift = hueNoise->evaluate(tile.position * 0.01) * 0.1;
		vec3 color = colorHueShift(vec3(172, 159, 139) / 255, hueShift);
		color = colorDeviation(color, 0.08);
		real roughness = 0.6 * randomChance() * 0.2;
		real metallic = 0;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateWater(Tile &tile)
	{
		static const Holder<NoiseFunction> xNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> yNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> zNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
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
		tile.metallic = interpolate(tile.metallic, 0, bf);
		tile.biome = TerrainBiomeEnum::Ocean;
		tile.type = shallow > 0.5 ? TerrainTypeEnum::ShallowWater : TerrainTypeEnum::DeepWater;

		{ // waves
			vec3 s = tile.position * 0.001;
			real x = xNoise->evaluate(s);
			real y = yNoise->evaluate(s);
			real z = zNoise->evaluate(s);
			vec3 dir = normalize(vec3(x, y, z));
			CAGE_ASSERT(isUnit(dir));
			real dist = dot(dir, normalize(tile.position)) * length(tile.position);
			real wave = sin(rads(dist * 0.005)) * 0.05 + 0.5;
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
	generateBedrock(tile); // 0 .. 1 -> 0.13
	// mica (slída)
	// dirt // 0.3 .. 0.7 -> 0.5
	generateSand(tile); // 0.3 .. 0.7 -> 0.5
	// corals
	// seaweed
	// boulders
	// tree stumps
	// small grass / moss / leaves
	generateWater(tile); // 0.45 .. 0.55 -> 0.5
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
	tile.position = vec3(0, 1, 0);
	tile.normal = vec3(0, 1, 0);
	terrainTile(tile);
}
