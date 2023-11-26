#include "math.h"
#include "planets.h"
#include "voronoi.h"

#include <cage-core/noiseFunction.h>

namespace unnatural
{
	Real elevationNone(const Vec3 &)
	{
		return 100;
	}

	Real elevationSimple(const Vec3 &pos)
	{
		static const Holder<NoiseFunction> elevNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::Ridged;
			cfg.octaves = 6;
			cfg.gain = 0.4;
			cfg.frequency = 0.0005;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real a = elevNoise->evaluate(pos);
		a = -a + 0.3; // min: -0.7, mean: 0.02, max: 1.1
		a = pow(a * 1.3 - 0.35, 3) + 0.1;
		return 100 - a * 1000;
	}

	Real elevationLegacy(const Vec3 &pos)
	{
		static const Holder<NoiseFunction> scaleNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.0005;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> elevNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real scale = scaleNoise->evaluate(pos) * 0.0005 + 0.0015;
		Real a = elevNoise->evaluate(pos * scale);
		a += 0.11; // slightly prefer terrain over ocean
		if (a < 0)
			a = -pow(-a, 0.85);
		else
			a = pow(a, 1.7);
		return a * 2500;
	}

	namespace
	{
		Real commonElevationMountains(const Vec3 &pos, Real land)
		{
			static const Holder<NoiseFunction> maskNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.frequency = 0.0015;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> ridgeNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Simplex;
				cfg.fractalType = NoiseFractalTypeEnum::Ridged;
				cfg.octaves = 4;
				cfg.lacunarity = 1.5;
				cfg.gain = -0.4;
				cfg.frequency = 0.001;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> terraceNoise = []()
			{
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Perlin;
				cfg.fractalType = NoiseFractalTypeEnum::Fbm;
				cfg.octaves = 3;
				cfg.gain = 0.3;
				cfg.frequency = 0.002;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();

			Real cover = 1 - saturate(land * -0.1);
			if (cover < 1e-7)
				return land;

			Real mask = maskNoise->evaluate(pos);
			Real rm = smoothstep(saturate(mask * +7 - 0.3));
			Real tm = smoothstep(saturate(mask * -7 - 1.5));

			Real ridge = ridgeNoise->evaluate(pos);
			ridge = max(ridge - 0.1, 0);
			ridge = pow(ridge, 1.6);
			ridge *= rm * cover;
			ridge *= 1000;

			Real terraces = terraceNoise->evaluate(pos);
			terraces = max(terraces + 0.1, 0) * 2.5;
			terraces = terrace(terraces, 4);
			terraces *= tm * cover;
			terraces *= 250;

			return land + smoothMax(0, max(ridge, terraces), 50);
		}
	}

	// lakes & islands
	// https://www.wolframalpha.com/input/?i=plot+%28%28%281+-+x%5E0.85%29+*+2+-+1%29+%2F+%28abs%28%28%281+-+x%5E0.85%29+*+2+-+1%29%29+%2B+0.17%29+%2B+0.15%29+*+150+%2C+%28%28%281+-+x%5E1.24%29+*+2+-+1%29+%2F+%28abs%28%28%281+-+x%5E1.24%29+*+2+-+1%29%29+%2B+0.17%29+%2B+0.15%29+*+150+%2C+x+%3D+0+..+1

	Real elevationLakes(const Vec3 &pos)
	{
		static const Holder<NoiseFunction> elevLand = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.0013;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real land = elevLand->evaluate(pos) * 0.5 + 0.5;
		land = saturate(land);
		land = 1 - pow(land, 1.24);
		land = land * 2 - 1;
		land = land / (abs(land) + 0.17) + 0.15;
		land *= 150;
		return commonElevationMountains(pos, land);
	}

	Real elevationIslands(const Vec3 &pos)
	{
		static const Holder<NoiseFunction> elevLand = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.0013;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		Real land = elevLand->evaluate(pos) * 0.5 + 0.5;
		land = saturate(land);
		land = 1 - pow(land, 0.83);
		land = land * 2 - 1;
		land = land / (abs(land) + 0.17) + 0.15;
		land *= 150;
		return commonElevationMountains(pos, land);
	}

	Real elevationCraters(const Vec3 &pos)
	{
		static const Holder<Voronoi> impactsNoise = []()
		{
			VoronoiCreateConfig cfg;
			cfg.cellSize = 300;
			cfg.pointsPerCell = 2;
			cfg.seed = noiseSeed();
			return newVoronoi(cfg);
		}();
		static const Holder<NoiseFunction> scaleNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.frequency = 1;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const auto &crater = [](Real x) { return (pow(x, 4) - 0.9 * pow(x, 5) - 0.1) * 10; }; // 0..1 -> -1..0.2

		const Vec3 c = impactsNoise->evaluate(pos, Vec3::Nan()).points[0];
		const Real s = 100 + scaleNoise->evaluate(c) * 50;
		const Real d = distance(pos, c);
		const Real h = crater(saturate(d / s));
		return 200 + h * 190;
	}
}
