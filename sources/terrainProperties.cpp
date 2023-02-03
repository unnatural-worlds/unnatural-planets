#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>
#include <cage-core/config.h>

#include "voronoi.h"
#include "terrain.h"
#include "generator.h"
#include "math.h"

namespace
{
	const ConfigBool configPolesEnable("unnatural-planets/poles/enable");

	// returns zero when the slope is at or above the threshold plus the smoothing,
	// returns one when the slope is at or below the threshold minus the smoothing
	Real steepnessMask(Degs slope, Degs threshold, Degs smoothing = Degs(10))
	{
		Degs r = (threshold + smoothing - slope) / (2 * smoothing);
		return sharpEdge(saturate(r.value));
	}

	Real rangeMask(Real value, Real zeroAt, Real oneAt)
	{
		return saturate((value - zeroAt) / (oneAt - zeroAt));
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
		Real p = elevNoise->evaluate(tile.position);
		Real m = maskNoise->evaluate(tile.position);
		m = 1 - smootherstep(abs(m));
		tile.elevation += p * m * 30;
	}

	void generatePrecipitation(Tile &tile)
	{
		static const Holder<NoiseFunction> precpNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.frequency = 0.001;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		Real p = precpNoise->evaluate(tile.position) * 0.5 + 0.5;
		p = saturate(p);
		p = smootherstep(p);
		p = smootherstep(p);
		p = pow(max(p + 0.2, 0), 3) * 0.6;
		p *= 400;
		p += max(120 - abs(tile.elevation), 0) * 0.2; // more water close to oceans
		p = max(p, 0);
		tile.precipitation = p;
	}

	void generateTemperature(Tile &tile)
	{
		static const Holder<NoiseFunction> tempNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.gain = 0.4;
			cfg.frequency = 0.0003;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> polarNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.frequency = 0.007;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real t = tempNoise->evaluate(tile.position) * 0.5 + 0.5;
		t = saturate(t);
		t = smoothstep(t);
		t = t * 2 - 1;

		if (configPolesEnable)
		{
			Real polar = abs(atan(tile.position[1] / length(Vec2(tile.position[0], tile.position[2]))).value) / Real::Pi() * 2;
			polar = pow(polar, 1.7);
			polar += polarNoise->evaluate(tile.position) * 0.1;
			t += 0.6 - polar * 3.2;
		}

		tile.temperature = 15 + t * 30 - max(tile.elevation, 0) * 0.02;
	}

	void generateWater(Tile &tile)
	{
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> waveNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::Ridged;
			cfg.octaves = 2;
			cfg.frequency = 0.15;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real shallow = rangeMask(tile.elevation, -20, 3);
		shallow = smoothstep(shallow);
		Real hueShift = hueNoise->evaluate(tile.position) * 0.06;
		Vec3 color = colorHueShift(interpolate(Vec3(54, 54, 97), Vec3(26, 102, 125), shallow) / 255, hueShift);

		tile.albedo = color;
		tile.roughness = 0.3;
		tile.metallic = 0;

		{ // waves
			Real w = waveNoise->evaluate(tile.position);
			w = pow(abs(w), 3);
			w = saturate(w);
			w *= 1 - shallow * 0.9;
			tile.height = w * 0.1 + 0.5;
			tile.albedo = interpolate(tile.albedo, Vec3(1), w * 0.5 * randomChance());
		}

		{
			Real d1 = 1 - sqr(rangeMask(tile.elevation, -10, 3));
			Real d2 = rescale(rangeMask(tile.elevation, 0, -200), 0, 1, 0.7, 0.95);
			tile.opacity = d1 * d2;
		}

		tile.biome = TerrainBiomeEnum::Water;
		tile.type = shallow > 0.5 ? TerrainTypeEnum::ShallowWater : TerrainTypeEnum::DeepWater;
	}

	void generateIce(Tile &tile)
	{
		static const Holder<NoiseFunction> temperatureOffsetNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.frequency = 0.05;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
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

		Real tempOff = temperatureOffsetNoise->evaluate(tile.position) * 1.5;
		Real bf = sharpEdge(rangeMask(tile.temperature + tempOff, 0, -3));
		if (bf < 1e-7)
			return;

		Real scale = scaleNoise->evaluate(tile.position) * 0.02 + 0.5;
		Real crack = cracksNoise->evaluate(tile.position * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.3);
		Vec3 color = Vec3(61, 81, 82) / 255 + crack * 0.3;
		Real roughness = (1 - crack) * 0.6 + 0.15;
		Real metallic = 0;
		Real height = crack * 0.2 + 0.4;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
		tile.opacity = interpolate(tile.opacity, tile.opacity + 0.1, bf);

		if (bf > 0.1)
		{
			tile.biome = TerrainBiomeEnum::Bare;
			if (tile.type != TerrainTypeEnum::SteepSlope)
				tile.type = TerrainTypeEnum::Rough;
		}
	}

	void generateSlope(Tile &tile)
	{
		constexpr Real radius = 0.5;
		Vec3 a = anyPerpendicular(tile.normal);
		Vec3 b = cross(tile.normal, a);
		a *= radius;
		b *= radius;
		const Real div = 1 / sqrt(2);
		Vec3 c = (a + b) * div;
		Vec3 d = (a - b) * div;
		Real elevs[8] = {
			terrainSdfElevation(tile.position + a),
			terrainSdfElevation(tile.position + b),
			terrainSdfElevation(tile.position - a),
			terrainSdfElevation(tile.position - b),
			terrainSdfElevation(tile.position + c),
			terrainSdfElevation(tile.position + d),
			terrainSdfElevation(tile.position - c),
			terrainSdfElevation(tile.position - d),
		};
		Real e1 = elevs[0];
		Real e2 = elevs[0];
		for (Real e : elevs)
		{
			e1 = min(e1, e);
			e2 = max(e2, e);
		}
		Real md = (e2 - e1) * 0.1;
		tile.slope = atan(md / radius);
	}

	void generateBiome(Tile &tile)
	{
		if (tile.elevation < 0)
			tile.biome = TerrainBiomeEnum::Water;
		else if (tile.temperature > 20)
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
		if (tile.elevation < -10)
			tile.type = TerrainTypeEnum::DeepWater;
		else if (tile.elevation < 0)
			tile.type = TerrainTypeEnum::ShallowWater;
		else if (tile.slope > Degs(20))
			tile.type = TerrainTypeEnum::SteepSlope;
		else switch (tile.biome)
		{
		case TerrainBiomeEnum::Bare:
		case TerrainBiomeEnum::Tundra:
			tile.type = TerrainTypeEnum::Rough;
			break;
		default:
			tile.type = TerrainTypeEnum::Flat;
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

		Real scale = scaleNoise->evaluate(tile.position) * 0.5 + 0.501;
		scale = sqr(scale) * 2;
		Real freq = freqNoise->evaluate(tile.position) * 0.05 + 0.15;
		Real cracks = cracksNoise->evaluate(tile.position * freq) * 0.5 + 0.5;
		cracks = saturate(pow(cracks, 0.8));
		Real value = valueNoise->evaluate(tile.position * freq) * 0.5 + 0.5;
		Real saturation = saturationNoise->evaluate(tile.position) * 0.5 + 0.5;
		Vec3 hsv = Vec3(0.07, saturate(sharpEdge(saturation, 0.2)), (value * 0.6 + 0.2) * cracks);
		tile.albedo = colorHsvToRgb(hsv);
		tile.roughness = interpolate(0.9, value * 0.2 + 0.7, cracks);
		tile.height = cracks * scale;
	}

	void generateCliffs(Tile &tile)
	{
		Real bf = saturate((Degs(tile.slope).value - 20) * 0.05);
		if (bf < 1e-7)
			return;

		Vec3 &color = tile.albedo;
		Vec3 hsv = colorRgbToHsv(color);
		hsv[1] *= bf;
		color = colorHsvToRgb(hsv);
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

		Real bf = saturate((maskNoise->evaluate(tile.position) - 1) * 10);
		if (bf < 1e-7)
			return;

		Real cracks = sharpEdge(saturate((cracksNoise->evaluate(tile.position) + 0.6)));
		Vec3 color = interpolate(Vec3(122, 90, 88) / 255, Vec3(184, 209, 187) / 255, cracks);
		Real roughness = cracks * 0.3 + 0.5;
		Real metallic = 1;
		Real height = tile.height - 0.05;

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

		Real height = heightNoise->evaluate(tile.position) * 0.2 + 0.5;
		Real bf = sharpEdge(saturate(height - tile.height + 0.4)) * steepnessMask(tile.slope, Degs(20));
		if (bf < 1e-7)
			return;

		Vec3 color = Vec3(84, 47, 14) / 255;
		{
			Real saturation = rangeMask(tile.precipitation, 0, 50);
			Vec3 hsv = colorRgbToHsv(color);
			hsv[1] *= saturation;
			color = colorHsvToRgb(hsv);
		}
		Real roughness = randomChance() * 0.1 + 0.7;
		Real metallic = 0;

		{ // cracks
			Real cracks = sharpEdge(saturate(cracksNoise->evaluate(tile.position) * 2 - 1.2), 0.15);
			cracks *= sqr(smoothstep(saturate(cracksMaskNoise->evaluate(tile.position) * 0.5 + 0.5))) * 0.9 + 0.1;
			cracks *= rangeMask(tile.precipitation, 70, 20) * 0.9 + 0.1;
			height = interpolate(height, height * 0.5, cracks);
			color = interpolate(color, Vec3(0.1), cracks);
			roughness = interpolate(roughness, 0.9, cracks);
		}

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateSand(Tile &tile)
	{
		static const Holder<NoiseFunction> heightScaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.frequency = 0.001;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> heightNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::Ridged;
			cfg.octaves = 3;
			cfg.gain = 0.7;
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> colorNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.0008;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.frequency = 0.02;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real bf = rangeMask(tile.temperature, 24, 28) * steepnessMask(tile.slope, Degs(19));
		if (bf < 1e-7)
			return;

		Real heightScale = heightScaleNoise->evaluate(tile.position) * 0.3 + 1;
		Real height = heightNoise->evaluate(tile.position * heightScale) * 0.2;
		height *= rangeMask(tile.precipitation, 100, 50) * 0.4 + 0.6;
		height += 0.5;
		Real hueShift = hueNoise->evaluate(tile.position) * 0.1;
		Vec3 color1 = colorHueShift(Vec3(172, 159, 139) / 255, hueShift);
		Real colorShift = smootherstep(smootherstep(colorNoise->evaluate(tile.position) * 0.5 + 0.5));
		Vec3 color = interpolateColor(color1, Vec3(170, 95, 46) / 255, colorShift);
		color = colorDeviation(color, 0.08);
		Real roughness = randomChance() * 0.3 + 0.6;
		Real metallic = sqr(sqr(randomChance()));

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
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (tile.biome == TerrainBiomeEnum::Water)
			return;

		Real bf = rangeMask(abs(tile.temperature - 14), 20, 15) * rangeMask(abs(tile.precipitation - 150), 120, 90) * steepnessMask(tile.slope, Degs(30));
		if (bf < 1e-7)
			return;

		Real bladesMask = 0;
		for (uint32 i = 0; i < sizeof(bladesNoise) / sizeof(bladesNoise[0]); i++)
		{
			const Real bn = bladesNoise[i]->evaluate(tile.position);
			bladesMask += sharpEdge(bf * 0.5 + 0.5 + bn);
		}

		bf *= saturate(bladesMask);
		if (bf < 1e-7)
			return;

		Real height = tile.height + bladesMask * 0.05;
		Real ratio = clamp(tile.temperature - (tile.precipitation + 100) * 30 / 400, 0, 5);
		Real hueShift = hueNoise->evaluate(tile.position) * 0.09 - ratio * 0.02;
		Vec3 color = colorHueShift(Vec3(79, 114, 55) / 255, hueShift);
		Real roughness = 0.7 + min(ratio, 0) * 0.02 + randomChance() * 0.2;
		Real metallic = rangeMask(tile.precipitation, 230, 270) * 0.1;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	template<bool Waterlily>
	void generateFlowers(Tile &tile)
	{
		static const Holder<Voronoi> clusterVoronoi = []() {
			VoronoiCreateConfig cfg;
			cfg.cellSize = 150;
			cfg.seed = noiseSeed();
			return newVoronoi(cfg);
		}();
		static const Holder<Voronoi> centerVoronoi = []() {
			VoronoiCreateConfig cfg;
			cfg.cellSize = 15;
			cfg.seed = noiseSeed();
			return newVoronoi(cfg);
		}();
		static const Holder<NoiseFunction> sizeNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.7;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> colorNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 1;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hue1Noise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hue2Noise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.8;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if ((tile.biome == TerrainBiomeEnum::Water) != Waterlily)
			return;

		if (rangeMask(abs(tile.temperature - 18), 12, 7) < 1e-5)
			return;

		if (!Waterlily && rangeMask(abs(tile.precipitation - 200), 100, 70) < 1e-5)
			return;

		const Vec3 center = centerVoronoi->evaluate(tile.position, tile.normal).points[0];
		if (distanceSquared(center, tile.position) > sqr(2))
			return;
		const Vec3 cluster = clusterVoronoi->evaluate(tile.position, tile.normal).points[0];
		if (distanceSquared(center, cluster) > sqr(10))
			return;

		Real dist = distance(center, tile.position);
		Real size = sizeNoise->evaluate(tile.position) * 0.5 + 0.5;
		size = smootherstep(smootherstep(saturate(size))) + 0.5;

		Real bf = rangeMask(size - dist, 0, 0.1);
		if (bf < 1e-7)
			return;

		Vec3 color = Waterlily ? Vec3(0.1, 0.7, 0) : colorNoise->evaluate(cluster) < 0.1 ? Vec3(1, 0, 0.7) : Vec3(1, 0.8, 0);
		color = colorHueShift(color, hue1Noise->evaluate(tile.position) * 0.1 + hue2Noise->evaluate(tile.position) * 0.1);
		Real roughness = 0.5;
		Real metallic = 0;
		Real height = 0.7 + sqr(dist / size) * 0.2;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateBoulders(Tile &tile)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.01;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<Voronoi> centerVoronoi = []() {
			VoronoiCreateConfig cfg;
			cfg.cellSize = 150;
			cfg.pointsPerCell = 2;
			cfg.seed = noiseSeed();
			return newVoronoi(cfg);
		}();
		static const Holder<NoiseFunction> sizeNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.3;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> valueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.8;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> scratchesNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 2;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (thresholdNoise->evaluate(tile.position) < 0.15)
			return;

		const auto centers = centerVoronoi->evaluate(tile.position, tile.normal);
		Vec3 center = centers.points[0];

		Real dist = distance(center, tile.position);
		Real size = sizeNoise->evaluate(tile.position) * 0.5 + 0.5;
		size = smootherstep(smootherstep(saturate(size))) * 2 + 0.5;

		Real bf = rangeMask(size - dist, 0, 0.1);
		if (bf < 1e-7)
			return;

		Real hueShift = hueNoise->evaluate(tile.position) * 0.07;
		Real valueShift = valueNoise->evaluate(tile.position) * 0.15;
		Vec3 color = colorRgbToHsv(Vec3(0.6));
		color[0] = (color[0] + hueShift + 1) % 1;
		color[2] = saturate(color[2] + valueShift);
		color = colorHsvToRgb(color);
		Real roughness = scratchesNoise->evaluate(tile.position) * 0.1 + 0.6;
		Real metallic = 0;
		Real height = 1 - sqr(dist / size) * 0.5;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
	}

	void generateTreeStumps(Tile &tile)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.02;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<Voronoi> centerVoronoi = []() {
			VoronoiCreateConfig cfg;
			cfg.cellSize = 40;
			cfg.seed = noiseSeed();
			return newVoronoi(cfg);
		}();
		static const Holder<NoiseFunction> sizeNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 1.5;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> hueNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.2;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		if (tile.type == TerrainTypeEnum::SteepSlope)
			return;

		switch (tile.biome)
		{
		case TerrainBiomeEnum::Taiga:
		case TerrainBiomeEnum::TemperateRainForest:
		case TerrainBiomeEnum::TemperateSeasonalForest:
		case TerrainBiomeEnum::TropicalRainForest:
		case TerrainBiomeEnum::TropicalSeasonalForest:
			break;
		default:
			return; // no trees here
		}

		if (thresholdNoise->evaluate(tile.position) < 0.1)
			return;

		const auto centers = centerVoronoi->evaluate(tile.position, tile.normal);
		Vec3 center = centers.points[0];

		Real dist = distance(center, tile.position);
		Real size = sizeNoise->evaluate(tile.position) * 0.5 + 0.5;
		size = smootherstep(saturate(size)) * 0.4 + 0.7;

		Real bf = rangeMask(size - dist, 0, 0.1);
		if (bf < 1e-7)
			return;

		Real hueShift = hueNoise->evaluate(tile.position) * 0.08;
		Vec3 baseColor = colorHueShift(Vec3(180, 146, 88) / 255, hueShift);
		Vec3 color = interpolate(Vec3(0.5), baseColor, rangeMask(size - dist, 0.2, 0.7));
		Real roughness = 0.8;
		Real metallic = 0;
		Real height = interpolate(height, 1, rangeMask(size - dist, 0, 0.3));

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);
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

		Real bf = rangeMask(abs(tile.temperature - 30), 15, 10) * rangeMask(tile.precipitation, 200, 250) * steepnessMask(tile.slope, Degs(22));
		if (bf < 1e-7)
			return;

		Real cracks = cracksNoise->evaluate(tile.position) * 0.5 + 0.5;
		cracks = saturate(pow(cracks, 0.4));
		bf *= cracks * 0.5 + 0.5;

		Real pores = saturate(pow(randomChance(), 0.4));
		Real height = interpolate(tile.height, 0.5, 0.5) + min(cracks, pores) * 0.05;
		Real hueShift = hueshiftNoise->evaluate(tile.position) * 0.07;
		Vec3 color = colorHueShift(Vec3(99, 147, 65) / 255, hueShift);
		color = interpolate(Vec3(76, 61, 50) / 255, color, pores);
		Real roughness = interpolate(0.9, randomChance() * 0.2 + 0.3, min(cracks, pores));
		Real metallic = rangeMask(tile.precipitation, 300, 400) * 0.1;

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

		Real bf = rangeMask(tile.temperature, 0, -2) * rangeMask(tile.precipitation, 50, 60) * steepnessMask(tile.slope, Degs(18));
		if (bf < 1e-7)
			return;
		Real factor = (thresholdNoise->evaluate(tile.position) * 0.5 + 0.5) * 0.5 + 0.7;
		bf *= saturate(factor);

		Vec3 color = Vec3(248) / 255;
		Real roughness = randomChance() * 0.3 + 0.2;
		Real metallic = 0;
		Real height = tile.height * 0.1 + factor * 0.2 + 0.7;

		tile.albedo = interpolate(tile.albedo, color, bf);
		tile.roughness = interpolate(tile.roughness, roughness, bf);
		tile.metallic = interpolate(tile.metallic, metallic, bf);
		tile.height = interpolate(tile.height, height, bf);

		if (bf > 0.1)
		{
			if (tile.type != TerrainTypeEnum::SteepSlope)
				tile.type = TerrainTypeEnum::Rough;
		}
	}

	void generateLand(Tile &tile)
	{
		generateElevation(tile);
		generatePrecipitation(tile);
		generateTemperature(tile);
		generateSlope(tile);
		generateBiome(tile);
		generateType(tile);
		generateBedrock(tile);
		generateCliffs(tile);
		generateMica(tile);
		generateDirt(tile);
		generateSand(tile);
		generateGrass(tile);
		generateFlowers<false>(tile);
		generateBoulders(tile);
		generateTreeStumps(tile);
		generateMoss(tile);
		generateSnow(tile);
	}

	void generateVisualization(Tile &tile)
	{
		generateElevation(tile);
		generatePrecipitation(tile);
		generateTemperature(tile);
		generateSlope(tile);
		generateBiome(tile);
		generateType(tile);

		tile.albedo = [&]() {
			switch (tile.biome)
			{
			case TerrainBiomeEnum::Bare: return Vec3(187) / 255;
			case TerrainBiomeEnum::Tundra: return Vec3(175, 226, 255) / 255;
			case TerrainBiomeEnum::Taiga: return Vec3(193, 225, 221) / 255;
			case TerrainBiomeEnum::Shrubland: return Vec3(200, 110, 62) / 255;
			case TerrainBiomeEnum::Grassland: return Vec3(239, 252, 143) / 255;
			case TerrainBiomeEnum::TemperateSeasonalForest: return Vec3(151, 183, 102) / 255;
			case TerrainBiomeEnum::TemperateRainForest: return Vec3(118, 169, 92) / 255;
			case TerrainBiomeEnum::Desert: return Vec3(221, 187, 76) / 255;
			case TerrainBiomeEnum::Savanna: return Vec3(253, 213, 120) / 255;
			case TerrainBiomeEnum::TropicalSeasonalForest: return Vec3(161, 151, 0) / 255;
			case TerrainBiomeEnum::TropicalRainForest: return Vec3(50, 122, 30) / 255;
			case TerrainBiomeEnum::Water: return Vec3(0, 0, 255) / 255;
			default: return Vec3::Nan();
			};
		}();

		tile.roughness = 0.7;
	}

	void generateFinalization(Tile &tile)
	{
		tile.albedo = saturate(tile.albedo);
		tile.roughness = saturate(tile.roughness);
		tile.metallic = saturate(tile.metallic);
		tile.height = saturate(tile.height);
		tile.opacity = saturate(tile.opacity);
	}
}

constexpr bool EnableVisualization = false;

void terrainTileLand(Tile &tile)
{
	CAGE_ASSERT(isUnit(tile.normal));
	tile.elevation = terrainSdfElevation(tile.position);
	if (EnableVisualization)
		generateVisualization(tile);
	else
		generateLand(tile);
	generateFinalization(tile);
}

void terrainTileWater(Tile &tile)
{
	CAGE_ASSERT(isUnit(tile.normal));
	tile.elevation = terrainSdfElevationRaw(tile.position);
	if (EnableVisualization)
	{
		generateVisualization(tile);
		tile.opacity = 0.5;
	}
	else
	{
		generateTemperature(tile);
		generateWater(tile);
		generateFlowers<true>(tile);
		generateIce(tile);
	}
	generateFinalization(tile);
}

void terrainTileNavigation(Tile &tile)
{
	CAGE_ASSERT(isUnit(tile.normal));
	{
		const Real l = terrainSdfElevation(tile.position);
		const Real w = terrainSdfElevationRaw(tile.position);
		tile.elevation = interpolate(w, l, rangeMask(l, 5, 10));
	}
	generateLand(tile);
	generateFinalization(tile);
}

void terrainPreseed()
{
	{
		terrainSdfLand(Vec3());
		Tile tile;
		tile.position = Vec3(0, 1, 0);
		tile.normal = Vec3(0, 1, 0);
		terrainTileLand(tile);
	}
	{
		terrainSdfWater(Vec3());
		Tile tile;
		tile.position = Vec3(0, 1, 0);
		tile.normal = Vec3(0, 1, 0);
		terrainTileWater(tile);
	}
}
