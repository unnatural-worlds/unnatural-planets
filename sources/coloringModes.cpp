#include "math.h"
#include "tile.h"
#include "voronoi.h"

#include <cage-core/color.h>
#include <cage-core/config.h>
#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>

namespace unnatural
{
	Real terrainSdfElevation(const Vec3 &pos);
	Real terrainSdfElevationRaw(const Vec3 &pos);

	namespace
	{
		const ConfigBool configPolesEnable("unnatural-planets/poles/enable");
		const ConfigBool configFlowersEnable("unnatural-planets/flowers/enable");

		// returns zero when the slope is at or above the threshold plus the smoothing,
		// returns one when the slope is at or below the threshold minus the smoothing
		Real steepnessMask(Degs slope, Degs threshold, Degs smoothing = Degs(10))
		{
			Degs r = (threshold + smoothing - slope) / (2 * smoothing);
			return saturate(r.value);
		}

		Real rangeMask(Real value, Real zeroAt, Real oneAt)
		{
			return saturate((value - zeroAt) / (oneAt - zeroAt));
		}

		Real beachMask(const Tile &tile)
		{
			static const Holder<NoiseFunction> beachNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.frequency = 0.035;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			if (tile.biome == TerrainBiomeEnum::Water)
				return 0;

			return rangeMask(tile.elevation + beachNoise->evaluate(tile.elevation) * 20, 15, 20);
		}

		void generateElevation(Tile &tile)
		{
			if (tile.meshPurpose == MeshPurposeEnum::Land)
				tile.elevation = terrainSdfElevation(tile.position);
			else
				tile.elevation = terrainSdfElevationRaw(tile.position);
		}

		void generatePrecipitation(Tile &tile)
		{
			static const Holder<NoiseFunction> precpNoise = []()
			{
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
			static const Holder<NoiseFunction> tempNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Simplex;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.gain = 0.4;
				cfg.frequency = 0.0003;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> polarNoise = []()
			{
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
			if (tile.elevation < -20)
				tile.type = TerrainTypeEnum::DeepWater;
			else if (tile.elevation < 0)
				tile.type = TerrainTypeEnum::ShallowWater;
			else if (tile.slope > Degs(20))
				tile.type = TerrainTypeEnum::SteepSlope;
			else
				switch (tile.biome)
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
			static const Holder<NoiseFunction> scaleNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cubic;
				cfg.frequency = 0.05;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> freqNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cubic;
				cfg.frequency = 0.02;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> cracksNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Subtract;
				cfg.frequency = 0.06;
				cfg.seed = seed;
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> valueNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Cell;
				cfg.frequency = 0.06;
				cfg.seed = seed;
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> saturationNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cubic;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.frequency = 0.007;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			const Real scale = sqr(scaleNoise->evaluate(tile.position) * 0.5 + 0.501) * 2;
			const Real freq = freqNoise->evaluate(tile.position) * 0.15 + 1;
			const Real cracks = saturate(pow(cracksNoise->evaluate(tile.position * freq) * 0.5 + 0.5, 0.8));
			const Real value = valueNoise->evaluate(tile.position * freq) * 0.5 + 0.6;
			const Real saturation = saturationNoise->evaluate(tile.position) * 0.5 + 0.6;
			const Vec3 hsv = Vec3(0.07, saturate(sharpEdge(saturation, 0.2)) * 0.7 + 0.1, (value * 0.2 + 0.3) * cracks);
			tile.albedo = colorHsvToRgb(hsv);
			tile.roughness = interpolate(0.9, value * 0.2 + 0.7, cracks);
			tile.height = cracks * scale;
		}

		void generateCliffs(Tile &tile)
		{
			const Real bf = steepnessMask(tile.slope, Degs(30), Degs(6));
			if (bf > 0.99999)
				return;

			Vec3 &color = tile.albedo;
			Vec3 hsv = colorRgbToHsv(color);
			hsv[1] *= bf;
			color = colorHsvToRgb(hsv);
		}

		void generateMica(Tile &tile)
		{
			static const Holder<NoiseFunction> maskNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Multiply;
				cfg.frequency = 0.02;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> cracksNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Manhattan;
				cfg.operation = NoiseOperationEnum::Divide;
				cfg.fractalType = NoiseFractalTypeEnum::None;
				cfg.frequency = 0.05;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			if (!configFlowersEnable)
				return;

			const Real bf = saturate((maskNoise->evaluate(tile.position) - 0.98) * 10);
			if (bf < 1e-7)
				return;

			const Real cracks = sharpEdge(saturate((cracksNoise->evaluate(tile.position) + 0.6)));
			const Vec3 color = interpolate(Vec3(122, 90, 88) / 255, Vec3(184, 209, 187) / 255, cracks);
			const Real roughness = cracks * 0.3 + 0.4;
			const Real metallic = 1;
			const Real height = tile.height * 0.5;

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, metallic, bf);
			tile.height = interpolate(tile.height, height, bf);
		}

		void generateDirt(Tile &tile)
		{
			static const Holder<NoiseFunction> heightNoise = []()
			{
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
			static const Holder<NoiseFunction> cracksNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::SimplexReduced;
				cfg.fractalType = NoiseFractalTypeEnum::Ridged;
				cfg.octaves = 2;
				cfg.frequency = 0.07;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> cracksMaskNoise = []()
			{
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

			Vec3 color = Vec3(168, 94, 28) / 255;
			{
				Real saturation = rangeMask(tile.precipitation, 0, 50);
				Vec3 hsv = colorRgbToHsv(color);
				hsv[1] *= saturation * 0.7;
				hsv[2] *= 0.8;
				color = colorHsvToRgb(hsv);
			}
			Real roughness = 0.8;
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
			static const Holder<NoiseFunction> heightScaleNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Simplex;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 3;
				cfg.frequency = 0.001;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> heightNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Simplex;
				cfg.fractalType = NoiseFractalTypeEnum::Ridged;
				cfg.octaves = 3;
				cfg.gain = 0.7;
				cfg.frequency = 0.01;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> colorNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.0008;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> hueNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cubic;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.frequency = 0.02;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			const Real bf = rangeMask(tile.temperature, 24, 28) * steepnessMask(tile.slope, Degs(19));
			if (bf < 1e-7)
				return;

			const Real heightScale = heightScaleNoise->evaluate(tile.position) * 0.3 + 1;
			const Real height = (heightNoise->evaluate(tile.position * heightScale) * 0.2) * (rangeMask(tile.precipitation, 100, 50) * 0.4 + 0.6) + 0.5;
			const Real colorShift = smootherstep(smootherstep(colorNoise->evaluate(tile.position) * 0.5 + 0.5));
			const Real hueShift = hueNoise->evaluate(tile.position) * 0.1;
			Vec3 color = colorHueShift(Vec3(189, 174, 152) / 255, hueShift);
			color = interpolateColor(color, Vec3(187, 104, 50) / 255, colorShift);
			color = colorDeviation(color, 0.08);
			color = colorHsvToRgb(colorRgbToHsv(color) * Vec3(1, 0.7, 0.8));
			const Real roughness = (heightScale - 1) * 0.7 + 0.55 + hueShift;
			const Real metallic = 0;

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, metallic, bf);
			tile.height = interpolate(tile.height, height, bf);
		}

		void generateGrass(Tile &tile)
		{
			static const uint32 seed = noiseSeed();
			static const Holder<NoiseFunction> hueNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.frequency = 0.01;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> patchesCellsNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Cell;
				cfg.frequency = 0.1;
				cfg.seed = seed;
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> patchesDistNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Subtract;
				cfg.frequency = 0.1;
				cfg.seed = seed;
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> scratchesNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Multiply;
				cfg.frequency = 0.3;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			const Real threshold = rangeMask(tile.temperature, 35, 25) * rangeMask(tile.precipitation, 15, 35) * steepnessMask(tile.slope, Degs(24), Degs(5));
			Real bf = rangeMask(threshold, 0.45, 0.55) * beachMask(tile);
			if (bf < 1e-7)
				return;

			const Real dryness = clamp(tile.temperature - (tile.precipitation + 100) * 30 / 400, 0, 5) / 5; // 0 .. 1
			const Real hueShiftBase = hueNoise->evaluate(tile.position);
			const Real hueShift = hueShiftBase * 0.09 - dryness * 0.1;
			Vec3 color = Vec3(114, 187, 77) / 255;
			color = colorHueShift(color, hueShift);
			const Real patchesMask = sharpEdge(patchesCellsNoise->evaluate(tile.position) * 0.5 + 0.2);
			const Real patches = pow(patchesDistNoise->evaluate(tile.position) * 0.5 + 0.5, 0.5) * patchesMask * (1 - dryness * 0.8);
			color *= 1 - patches * 0.09;
			const Real scratches = saturate(scratchesNoise->evaluate(tile.position) - 0.2) * (1 - dryness * 0.5);
			color *= 1 - scratches * 0.1;
			const Real roughness = 0.6 + dryness * 0.1 + patches * 0.1 + scratches * 0.1 - hueShiftBase * 0.1;
			const Real metallic = 0;
			const Real height = interpolate(tile.height, 0.5, 0.7) + (0.5 - dryness) * 0.2 + patches * 0.05 - scratches * 0.02 + hueShiftBase * 0.1 + 0.1;

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, metallic, bf);
			tile.height = interpolate(tile.height, height, bf);
		}

		void generateBoulders(Tile &tile)
		{
			static const Holder<NoiseFunction> thresholdNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.01;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<Voronoi> centerVoronoi = []()
			{
				VoronoiCreateConfig cfg;
				cfg.cellSize = 200;
				cfg.pointsPerCell = 2;
				cfg.seed = noiseSeed();
				return newVoronoi(cfg);
			}();
			static const Holder<NoiseFunction> sizeNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.09;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> hueNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.3;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> valueNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.5;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> scratchesNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.5;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			if (!configFlowersEnable)
				return;

			if (thresholdNoise->evaluate(tile.position) < 0.15)
				return;

			const auto centers = centerVoronoi->evaluate(tile.position, tile.normal);
			const Vec3 center = centers.points[0];
			const Real dist = distance(center, tile.position);
			const Real size = smootherstep(smootherstep(saturate(sizeNoise->evaluate(tile.position) * 0.5 + 0.5))) * 3 + 3;

			const Real bf = rangeMask(size - dist, 0, 0.5);
			if (bf < 1e-7)
				return;

			const Real hueShift = hueNoise->evaluate(tile.position) * 0.12;
			const Real valueShift = valueNoise->evaluate(tile.position) * 0.18;
			Vec3 color = colorRgbToHsv(Vec3(154) / 255);
			color[0] = (color[0] + hueShift + 1) % 1;
			color[2] = saturate(color[2] + valueShift);
			color = colorHsvToRgb(color);
			const Real roughness = scratchesNoise->evaluate(tile.position) * 0.1 + 0.4;
			const Real metallic = 0;
			const Real height = 1 - sqr(dist / size) * 0.5;

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, metallic, bf);
			tile.height = interpolate(tile.height, height, bf);
		}

		void generateWater(Tile &tile)
		{
			static const Holder<NoiseFunction> colorSwitchNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.frequency = 0.0015;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> hueShiftNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cubic;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 3;
				cfg.frequency = 0.004;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			{
				const Real colorSwitch = colorSwitchNoise->evaluate(tile.position) * 0.5 + 0.5;
				const Real hueShift = hueShiftNoise->evaluate(tile.position) * 0.09;
				tile.albedo = colorHueShift(interpolateColor(Vec3(54, 54, 97) / 255, Vec3(26, 102, 125) / 255, colorSwitch), hueShift);
			}

			{
				const Real d1 = 1 - sqr(rangeMask(tile.elevation, -25, 3));
				const Real d2 = rescale(rangeMask(tile.elevation, 0, -100), 0, 1, 0.7, 1);
				tile.opacity = d1 * d2;
			}

			{
				const Real deep = smoothstep(rangeMask(tile.elevation, 3, -30));
				tile.metallic = deep; // signal to apply dynamic waves in the shader
			}
		}

		void generateFlowers(Tile &tile)
		{
			static const Holder<Voronoi> clusterVoronoi = []()
			{
				VoronoiCreateConfig cfg;
				cfg.cellSize = 200;
				cfg.seed = noiseSeed();
				return newVoronoi(cfg);
			}();
			static const Holder<Voronoi> centerVoronoi = []()
			{
				VoronoiCreateConfig cfg;
				cfg.cellSize = 35;
				cfg.seed = noiseSeed();
				return newVoronoi(cfg);
			}();
			static const Holder<NoiseFunction> sizeNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.35;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> colorNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.02;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> hue1Noise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.005;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> hue2Noise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			if (!configFlowersEnable)
				return;

			const bool waterlily = tile.meshPurpose != MeshPurposeEnum::Land;
			if ((tile.biome == TerrainBiomeEnum::Water) != waterlily)
				return;

			if (rangeMask(abs(tile.temperature - 18), 12, 7) < 1e-5)
				return;

			if (!waterlily && rangeMask(abs(tile.precipitation - 200), 100, 70) < 1e-5)
				return;

			const Vec3 center = centerVoronoi->evaluate(tile.position, tile.normal).points[0];
			if (distanceSquared(center, tile.position) > sqr(4))
				return;
			const Vec3 cluster = clusterVoronoi->evaluate(tile.position, tile.normal).points[0];
			if (distanceSquared(center, cluster) > sqr(20))
				return;
			const Real dist = distance(center, tile.position);
			const Real size = smootherstep(smootherstep(saturate(sizeNoise->evaluate(tile.position) * 0.5 + 0.5))) * 2 + 1.5;

			const Real bf = rangeMask(size - dist, 0, 0.1);
			if (bf < 1e-7)
				return;

			const Vec3 baseColor = waterlily ? Vec3(0.2, 0.14, 0) : interpolateColor(Vec3(0.5, 0, 0.35), Vec3(0.5, 0.4, 0), sharpEdge(colorNoise->evaluate(cluster) + 0.4, 0.2));
			const Vec3 color = colorHueShift(baseColor, hue1Noise->evaluate(tile.position) * 0.1 + hue2Noise->evaluate(tile.position) * 0.1);
			const Real roughness = 0.8;
			const Real metallic = waterlily ? 0.85 : 0; // signal to apply dynamic waves in the shader
			const Real height = 0.7 + sqr(dist / size) * 0.2;

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, metallic, bf);
			tile.height = interpolate(tile.height, height, bf);
			tile.opacity = interpolate(tile.opacity, 1, bf);
		}

		void generateIce(Tile &tile)
		{
			static const uint32 seed = noiseSeed();
			static const Holder<NoiseFunction> temperatureOffsetNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.frequency = 0.035;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> freqNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 3;
				cfg.frequency = 0.01;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> thicknessNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Cell;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.frequency = 0.05;
				cfg.seed = seed;
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> cracksNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cellular;
				cfg.distance = NoiseDistanceEnum::Hybrid;
				cfg.operation = NoiseOperationEnum::Subtract;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 2;
				cfg.frequency = 0.05;
				cfg.seed = seed;
				return newNoiseFunction(cfg);
			}();

			const Real tempOff = temperatureOffsetNoise->evaluate(tile.position) * 1.5;
			const Real bf = sharpEdge(rangeMask(tile.temperature + tempOff, 0, -3));
			if (bf < 1e-7)
				return;

			if (bf > 0.1)
			{
				if (tile.type != TerrainTypeEnum::SteepSlope)
					tile.type = TerrainTypeEnum::Rough;
			}

			if (tile.meshPurpose == MeshPurposeEnum::Land)
				return;

			const Real freq = freqNoise->evaluate(tile.position) * 0.08 + 1;
			const Real thickness = sharpEdge(saturate(thicknessNoise->evaluate(tile.position * freq) * 0.5 + 0.8), 0.1) * 0.6 + 0.4; // 0.4 .. 1
			const Real crack = 1 - pow(cracksNoise->evaluate(tile.position * freq) * 0.5 + 0.5, 0.3);
			Vec3 color = max(Vec3(122, 162, 164) / 255 - crack * 0.3, 0);
			color = interpolateColor(tile.albedo, color, thickness);
			Real roughness = 0.15 + crack * 0.6;
			roughness = interpolate(tile.roughness, roughness, thickness);
			const Real height = thickness * 0.5 * (1 - crack * 0.8);

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, 0, bf); // signal to apply dynamic waves in the shader
			tile.height = interpolate(tile.height, height, bf);
			tile.opacity = interpolate(tile.opacity, tile.opacity * 0.9 + thickness * 0.5, bf);
		}

		void generateSnow(Tile &tile)
		{
			static const Holder<NoiseFunction> tempOffsetNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.frequency = 0.015;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> slopeOffsetNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.frequency = 0.11;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> thicknessNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Cubic;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 3;
				cfg.frequency = 0.05;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			Real bf = sharpEdge(rangeMask(tile.temperature + tempOffsetNoise->evaluate(tile.position) * 1.5, 0, -2) * rangeMask(tile.precipitation, 10, 15) * steepnessMask(tile.slope, Degs(20 + slopeOffsetNoise->evaluate(tile.position) * 3), Degs(3)) * beachMask(tile));
			const Real thickness = thicknessNoise->evaluate(tile.position) * 0.5 + 0.5;
			bf *= saturate(thickness * 0.5 + 0.7);
			if (bf < 1e-7)
				return;

			if (bf > 0.2)
			{
				if (tile.type != TerrainTypeEnum::SteepSlope)
					tile.type = TerrainTypeEnum::Rough;
			}

			if (tile.meshPurpose == MeshPurposeEnum::Water)
				return;

			const Vec3 color = Vec3(230) / 255;
			const Real roughness = thickness * 0.3 + 0.2;
			const Real metallic = 0;
			const Real height = 0.6 + tile.height * 0.1 + thickness * 0.3;

			tile.albedo = interpolate(tile.albedo, color, bf);
			tile.roughness = interpolate(tile.roughness, roughness, bf);
			tile.metallic = interpolate(tile.metallic, metallic, bf);
			tile.height = interpolate(tile.height, height, bf);
		}

		void generateVisualization(Tile &tile)
		{
			tile.albedo = [&]()
			{
				switch (tile.biome)
				{
					case TerrainBiomeEnum::Bare:
						return Vec3(187) / 255;
					case TerrainBiomeEnum::Tundra:
						return Vec3(175, 226, 255) / 255;
					case TerrainBiomeEnum::Taiga:
						return Vec3(193, 225, 221) / 255;
					case TerrainBiomeEnum::Shrubland:
						return Vec3(200, 110, 62) / 255;
					case TerrainBiomeEnum::Grassland:
						return Vec3(239, 252, 143) / 255;
					case TerrainBiomeEnum::TemperateSeasonalForest:
						return Vec3(151, 183, 102) / 255;
					case TerrainBiomeEnum::TemperateRainForest:
						return Vec3(118, 169, 92) / 255;
					case TerrainBiomeEnum::Desert:
						return Vec3(221, 187, 76) / 255;
					case TerrainBiomeEnum::Savanna:
						return Vec3(253, 213, 120) / 255;
					case TerrainBiomeEnum::TropicalSeasonalForest:
						return Vec3(161, 151, 0) / 255;
					case TerrainBiomeEnum::TropicalRainForest:
						return Vec3(50, 122, 30) / 255;
					case TerrainBiomeEnum::Water:
						return Vec3(0, 0, 255) / 255;
					default:
						return Vec3::Nan();
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

	void coloringDefault(Tile &tile)
	{
		generateElevation(tile);
		generatePrecipitation(tile);
		generateTemperature(tile);
		generateSlope(tile);
		generateBiome(tile);
		generateType(tile);
		if (tile.meshPurpose == MeshPurposeEnum::Water)
			generateWater(tile);
		else
		{
			generateBedrock(tile);
			generateCliffs(tile);
			generateMica(tile);
			generateDirt(tile);
			generateSand(tile);
			generateGrass(tile);
			generateBoulders(tile);
		}
		generateFlowers(tile);
		generateIce(tile);
		generateSnow(tile);
		generateFinalization(tile);
	}

	void coloringBarren(Tile &tile)
	{
		generateElevation(tile);
		generateSlope(tile);
		if (tile.meshPurpose == MeshPurposeEnum::Water)
		{
			tile.biome = TerrainBiomeEnum::Water;
			tile.type = TerrainTypeEnum::ShallowWater;
			generateWater(tile);
		}
		else
		{
			tile.biome = TerrainBiomeEnum::Bare;
			tile.type = TerrainTypeEnum::Rough;
			generateBedrock(tile);
			generateCliffs(tile);
			generateMica(tile);
			generateBoulders(tile);
		}
		generateFinalization(tile);
	}

	void coloringDebug(Tile &tile)
	{
		generateElevation(tile);
		generatePrecipitation(tile);
		generateTemperature(tile);
		generateSlope(tile);
		generateBiome(tile);
		generateType(tile);
		generateVisualization(tile);
		if (tile.meshPurpose == MeshPurposeEnum::Water)
			tile.opacity = 0.5;
		generateFinalization(tile);
	}
}
