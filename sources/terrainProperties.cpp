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
	ConfigBool configPolesEnable("unnatural-planets/poles/enable");

	real steepnessMask(degs slope, degs threshold = degs(20), degs smoothing = degs(5))
	{
		degs r = (threshold + smoothing - slope) / (2 * smoothing);
		return sharpEdge(saturate(r.value));
	}

	void generateElevation(Tile &tile)
	{
		static const Holder<NoiseFunction> elevNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.frequency = 0.1;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> maskNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.frequency = 0.005;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = elevNoise->evaluate(tile.position);
		real m = maskNoise->evaluate(tile.position);
		m = 1 - smootherstep(abs(m));
		tile.elevation += p * m * 0.3;
	}

	void generatePrecipitation(Tile &tile)
	{
		static const Holder<NoiseFunction> precpNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.0015;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = precpNoise->evaluate(tile.position) * 0.5 + 0.5;
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
			cfg.frequency = 0.0015;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real t = tempNoise->evaluate(tile.position) * 0.5 + 0.5;
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
			cfg.frequency = 0.007;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (!configPolesEnable)
			return;

		real polar = abs(atan(tile.position[1] / length(vec2(tile.position[0], tile.position[2]))).value) / real::Pi() * 2;
		polar = pow(polar, 1.7);
		polar += polarNoise->evaluate(tile.position) * 0.1;
		tile.temperature += 15 - polar * 80;
	}

	void generateWater(Tile &tile)
	{
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> xNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.frequency = 0.001;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> yNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.frequency = 0.001;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> zNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.frequency = 0.001;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real shallow = -0.5 / (tile.elevation - 0.5);
		shallow = saturate(shallow);
		shallow = smoothstep(shallow);
		real hueShift = hueNoise->evaluate(tile.position) * 0.06;
		vec3 color = colorHueShift(interpolate(vec3(54, 54, 97), vec3(26, 102, 125), shallow) / 255, hueShift);

		tile.albedo = color;
		tile.roughness = 0.3;
		tile.metallic = 0;

		{ // waves
			real x = xNoise->evaluate(tile.position);
			real y = yNoise->evaluate(tile.position);
			real z = zNoise->evaluate(tile.position);
			vec3 dir = normalize(vec3(x, y, z));
			CAGE_ASSERT(isUnit(dir));
			real dist = dot(dir, tile.position) * length(tile.position);
			rads a = rads(dist * 0.002);
			rads b = rads(sin(a + sin(a) * 0.5)); // skewed wave
			real c = sin(b + sin(b) * 0.5);
			real wave = c * 0.1 + 0.5;
			tile.height = wave;
		}

		tile.biome = TerrainBiomeEnum::Water;
		tile.type = shallow > 0.5 ? TerrainTypeEnum::ShallowWater : TerrainTypeEnum::DeepWater;
	}

	void generateSlope(Tile &tile)
	{
		constexpr real radius = 0.5;
		vec3 a = anyPerpendicular(tile.normal);
		vec3 b = cross(tile.normal, a);
		a *= radius;
		b *= radius;
		const real div = 1 / sqrt(2);
		vec3 c = (a + b) * div;
		vec3 d = (a - b) * div;
		real elevs[8] = {
			terrainSdfElevation(tile.position + a),
			terrainSdfElevation(tile.position + b),
			terrainSdfElevation(tile.position - a),
			terrainSdfElevation(tile.position - b),
			terrainSdfElevation(tile.position + c),
			terrainSdfElevation(tile.position + d),
			terrainSdfElevation(tile.position - c),
			terrainSdfElevation(tile.position - d),
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
			cfg.frequency = 0.05;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> freqNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.005;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> valueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Cell;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> saturationNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.003;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real scale = scaleNoise->evaluate(tile.position) * 0.5 + 0.501;
		scale = sqr(scale) * 2;
		real freq = freqNoise->evaluate(tile.position) * 0.05 + 0.15;
		real cracks = cracksNoise->evaluate(tile.position * freq) * 0.5 + 0.5;
		cracks = saturate(pow(cracks, 0.8));
		real value = valueNoise->evaluate(tile.position * freq) * 0.5 + 0.5;
		real saturation = saturationNoise->evaluate(tile.position) * 0.5 + 0.5;
		vec3 hsv = vec3(0.07, saturate(sharpEdge(saturation, 0.2)), (value * 0.6 + 0.2) * cracks);
		tile.albedo = colorHsvToRgb(hsv);
		tile.roughness = interpolate(0.9, value * 0.2 + 0.7, cracks);
		tile.height = cracks * scale;
	}

	void generateMica(Tile &tile)
	{
		static const Holder<NoiseFunction> maskNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Multiply;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.frequency = 0.1;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Manhattan;
			cfg.operation = NoiseOperationEnum::Divide;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.frequency = 0.3;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real bf = saturate((maskNoise->evaluate(tile.position) - 1) * 10);
		if (bf < 1e-7)
			return;

		real cracks = sharpEdge(saturate((cracksNoise->evaluate(tile.position) + 0.6)));
		vec3 color = interpolate(vec3(122, 90, 88) / 255, vec3(184, 209, 187) / 255, cracks);
		real roughness = cracks * 0.3 + 0.5;
		real metallic = 1;
		real height = tile.height - 0.05;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateDirt(Tile &tile)
	{
		static const Holder<NoiseFunction> heightNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.lacunarity = 2.3;
			cfg.gain = 0.4;
			cfg.frequency = 0.05;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::SimplexReduced;
			cfg.fractalType = NoiseFractalTypeEnum::Ridged;
			cfg.octaves = 2;
			cfg.frequency = 0.07;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> cracksMaskNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.02;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real height = heightNoise->evaluate(tile.position) * 0.2 + 0.5;
		real bf = sharpEdge(saturate(height - tile.height + 0.4)) * steepnessMask(tile.slope);
		if (bf < 1e-7)
			return;

		vec3 color = vec3(84, 47, 14) / 255;
		real roughness = randomChance() * 0.1 + 0.7;
		real metallic = 0;

		{ // cracks
			real cracks = sharpEdge(saturate(cracksNoise->evaluate(tile.position) * 2 - 1.2), 0.15);
			cracks *= sqr(smoothstep(saturate(cracksMaskNoise->evaluate(tile.position) * 0.5 + 0.5))) * 0.9 + 0.1;
			cracks *= saturate((50 - tile.precipitation) * 0.01);
			height = interpolate(height, height * 0.5, cracks);
			color = interpolate(color, vec3(0.1), cracks);
			roughness = interpolate(roughness, 0.9, cracks);
		}

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateSand(Tile &tile)
	{
		static const Holder<NoiseFunction> heightNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.gain = 0.3;
			cfg.frequency = 0.04;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real bf = saturate((tile.temperature - 17.5) * 0.2) * saturate((35 - tile.precipitation) * 0.1) * steepnessMask(tile.slope);
		if (bf < 1e-7)
			return;

		real height = heightNoise->evaluate(tile.position) * 0.2 + 0.5;
		real hueShift = hueNoise->evaluate(tile.position) * 0.1;
		vec3 color = colorHueShift(vec3(172, 159, 139) / 255, hueShift);
		color = colorDeviation(color, 0.08);
		real roughness = randomChance() * 0.3 + 0.6;
		real metallic = 0;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateIce(Tile &tile)
	{
		static const Holder<NoiseFunction> scaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.03;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.frequency = 0.1;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (tile.biome != TerrainBiomeEnum::Water)
			return;

		real bf = sharpEdge(saturate(tile.temperature * -0.3));
		if (bf < 1e-7)
			return;

		real scale = scaleNoise->evaluate(tile.position) * 0.02 + 0.5;
		real crack = cracksNoise->evaluate(tile.position * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.3);
		vec3 color = vec3(61, 81, 82) / 255 + crack * 0.3;
		real roughness = (1 - crack) * 0.6 + 0.15;
		real metallic = 0;
		real height = crack * 0.2 + 0.4;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);

		if (bf > 0.1)
		{
			tile.biome = TerrainBiomeEnum::Bare;
			if (tile.type != TerrainTypeEnum::SteepSlope)
				tile.type = TerrainTypeEnum::Slow;
		}
	}

	void generateMoss(Tile &tile)
	{
		static const uint32 seed = noiseSeed();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.gain = 0.3;
			cfg.frequency = 0.3;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hueshiftNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Hybrid;
			cfg.operation = NoiseOperationEnum::Cell;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.frequency = 0.3;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();

		if (tile.biome == TerrainBiomeEnum::Water)
			return;

		real bf = saturate((tile.temperature - 10) * 0.1) * saturate((tile.precipitation - 200) * 0.02) * steepnessMask(tile.slope);
		if (bf < 1e-7)
			return;

		real cracks = cracksNoise->evaluate(tile.position) * 0.5 + 0.5;
		cracks = saturate(pow(cracks, 0.4));
		bf *= cracks * 0.5 + 0.5;

		real pores = saturate(pow(randomChance(), 0.4));
		real height = tile.height + min(cracks, pores) * 0.05;
		real hueShift = hueshiftNoise->evaluate(tile.position) * 0.07;
		vec3 color = colorHueShift(vec3(99, 147, 65) / 255, hueShift);
		color = interpolate(vec3(76, 61, 50) / 255, color, pores);
		real roughness = interpolate(0.9, randomChance() * 0.2 + 0.3, min(cracks, pores));
		real metallic = 0;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateGrass(Tile &tile)
	{
		constexpr const auto bladesNoiseGen = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.fractalType = NoiseFractalTypeEnum::None;
			cfg.distance = NoiseDistanceEnum::EuclideanSq;
			cfg.operation = NoiseOperationEnum::Divide;
			cfg.frequency = 1.4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		};
		static const Holder<NoiseFunction> bladesNoise[] = {
			bladesNoiseGen(),
			bladesNoiseGen(),
			bladesNoiseGen(),
		};
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.frequency = 0.05;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (tile.biome == TerrainBiomeEnum::Water)
			return;

		real ratio = tile.temperature - (tile.precipitation + 100) * 30 / 400;
		real bf = sharpEdge(saturate(1 - abs(tile.temperature - 15) * 0.07), 0.2) * sharpEdge(saturate(1 - abs(ratio) * 0.1), 0.2) * steepnessMask(tile.slope);
		if (bf < 1e-7)
			return;

		real grass = 0;
		for (uint32 i = 0; i < sizeof(bladesNoise) / sizeof(bladesNoise[0]); i++)
			grass += sharpEdge(bladesNoise[i]->evaluate(tile.position) + 0.7);
		bf *= saturate(grass);

		real height = tile.height + grass * 0.05;
		real hueShift = hueNoise->evaluate(tile.position) * 0.09 - max(ratio, 0) * 0.02;
		vec3 color = colorHueShift(vec3(79, 114, 55) / 255, hueShift);
		real roughness = randomChance() * 0.2 + 0.6 + min(ratio, 0) * 0.03;
		real metallic = 0;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateSnow(Tile &tile)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.frequency = 0.1;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (tile.biome == TerrainBiomeEnum::Water)
			return;

		real bf = saturate(tile.temperature * -0.5) * saturate((tile.precipitation - 50) * 0.1) * steepnessMask(tile.slope);
		if (bf < 1e-7)
			return;
		real factor = (thresholdNoise->evaluate(tile.position) * 0.5 + 0.5) * 0.5 + 0.7;
		bf *= saturate(factor);

		vec3 color = vec3(248) / 255;
		real roughness = randomChance() * 0.3 + 0.2;
		real metallic = 0;
		real height = tile.height * 0.1 + factor * 0.2 + 0.7;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);

		if (bf > 0.1)
		{
			if (tile.type != TerrainTypeEnum::SteepSlope)
				tile.type = TerrainTypeEnum::Slow;
		}
	}
}

void terrainTile(Tile &tile, bool water)
{
	CAGE_ASSERT(isUnit(tile.normal));
	tile.elevation = terrainSdfElevation(tile.position);
	generateElevation(tile);
	generatePrecipitation(tile);
	generateTemperature(tile);
	generatePoles(tile);
	if (water)
	{
		generateWater(tile);
		generateIce(tile);
	}
	else
	{
		generateSlope(tile);
		generateBiome(tile);
		generateType(tile);
		generateBedrock(tile);
		generateMica(tile);
		generateDirt(tile);
		generateSand(tile);
		// corals
		// seaweed
		generateIce(tile);
		// boulders
		// tree stumps
		generateMoss(tile);
		generateGrass(tile);
		// leaves
		// flowers
		generateSnow(tile);
	}
	tile.albedo = saturate(tile.albedo);
	tile.roughness = saturate(tile.roughness);
	tile.metallic = saturate(tile.metallic);
	tile.height = saturate(tile.height);
}

void terrainPreseed()
{
	terrainSdfNavigation(vec3());
	Tile tile;
	tile.position = vec3(0, 1, 0);
	tile.normal = vec3(0, 1, 0);
	terrainTile(tile, false);
	terrainTile(tile, true);
}
