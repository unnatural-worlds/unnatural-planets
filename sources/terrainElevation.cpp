#include <cage-core/noiseFunction.h>
#include <cage-core/config.h>

#include "terrain.h"
#include "sdf.h"
#include "math.h"

namespace
{
	ConfigString configShapeMode("unnatural-planets/shape/mode");
	ConfigBool configElevationEnable("unnatural-planets/elevation/enable");
	ConfigString configElevationMode("unnatural-planets/elevation/mode");

	typedef real (*TerrainFunctor)(const vec3 &);
	TerrainFunctor terrainElevationFnc = 0;
	TerrainFunctor terrainShapeFnc = 0;

	real elevationNone(const vec3 &)
	{
		return 1;
	}

	real elevationSimplex(const vec3 &pos)
	{
		static const Holder<NoiseFunction> elevNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::SimplexReduced;
			cfg.frequency = 0.001;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real a = elevNoise->evaluate(pos);
		a += 0.5; // more land than water
		return a * 10;
	}

	real elevationLegacy(const vec3 &pos)
	{
		static const Holder<NoiseFunction> scaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.frequency = 0.0005;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> elevNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real scale = scaleNoise->evaluate(pos) * 0.0005 + 0.0015;
		real a = elevNoise->evaluate(pos * scale);
		a += 0.11; // slightly prefer terrain over ocean
		if (a < 0)
			a = -pow(-a, 0.85);
		else
			a = pow(a, 1.7);
		return a * 25;
	}

	real elevationTerraces(const vec3 &pos)
	{
		static const Holder<NoiseFunction> baseNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.gain = 0.3;
			cfg.frequency = 0.0008;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> clifsNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.0004;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		/*
		static const Holder<NoiseFunction> rampsNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.0015;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		*/
		static const Holder<NoiseFunction> levelNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Perlin;
			cfg.frequency = 0.0013;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		real b = baseNoise->evaluate(pos) / 0.62 * 0.5 + 0.5;
		real c = clifsNoise->evaluate(pos) / 0.8 * 0.5 + 0.5;
		c = c * 2 + 3;
		//real r = rampsNoise->evaluate(pos) / 0.75 * 0.5 + 0.5;
		//r = sharpEdge(saturate(r), 0.1) * 3;
		real r = 5;
		real e = terrace(b * c, r) / c;
		real l = levelNoise->evaluate(pos) / 0.8 * 0.5 + 0.5;
		e +=  2 * l / c;
		return (e - 0.5) * 30;
	}

	real elevationEarth(const vec3 &pos)
	{
		return 1; // todo
	}

	void chooseElevationFunction()
	{
		constexpr TerrainFunctor elevationModeFunctions[] = {
			&elevationNone,
			&elevationSimplex,
			&elevationLegacy,
			&elevationTerraces,
			&elevationEarth,
		};

		constexpr uint32 elevationModesCount = sizeof(elevationModeFunctions) / sizeof(elevationModeFunctions[0]);

		constexpr const char *const elevationModeNames[] = {
			"none",
			"simplex",
			"legacy",
			"terraces",
			"earth",
		};

		static_assert(elevationModesCount == sizeof(elevationModeNames) / sizeof(elevationModeNames[0]), "number of functions and names must match");

		for (uint32 i = 0; i < elevationModesCount; i++)
			if ((string)configElevationMode == elevationModeNames[i])
				terrainElevationFnc = elevationModeFunctions[i];
		if (!terrainElevationFnc)
		{
			CAGE_LOG_THROW(stringizer() + "elevation mode: '" + (string)configElevationMode + "'");
			CAGE_THROW_ERROR(Exception, "unknown elevation mode configuration");
		}
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using elevation mode: '" + (string)configElevationMode + "'");
	}

	void chooseShapeFunction()
	{
		constexpr TerrainFunctor shapeModeFunctions[] = {
			&sdfHexagon,
			&sdfSquare,
			&sdfSphere,
			&sdfTorus,
			&sdfTube,
			&sdfDisk,
			&sdfCapsule,
			&sdfBox,
			&sdfCube,
			&sdfTetrahedron,
			&sdfOctahedron,
			&sdfKnot,
			&sdfMobiusStrip,
			&sdfMolecule,
			&sdfH2O,
			&sdfH3O,
			&sdfH4O,
			&sdfTriangularPrism,
			&sdfHexagonalPrism,
		};

		constexpr uint32 shapeModesCount = sizeof(shapeModeFunctions) / sizeof(shapeModeFunctions[0]);

		constexpr const char *const shapeModeNames[] = {
			"hexagon",
			"square",
			"sphere",
			"torus",
			"tube",
			"disk",
			"capsule",
			"box",
			"cube",
			"tetrahedron",
			"octahedron",
			"knot",
			"mobiusstrip",
			"molecule",
			"h2o",
			"h3o",
			"h4o",
			"triangularprism",
			"hexagonalprism",
		};

		static_assert(shapeModesCount == sizeof(shapeModeNames) / sizeof(shapeModeNames[0]), "number of functions and names must match");

		string name = configShapeMode;
		if (name == "random")
		{
			const uint32 i = randomRange(0u, shapeModesCount);
			terrainShapeFnc = shapeModeFunctions[i];
			configShapeMode = name = shapeModeNames[i];
			CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "randomly chosen shape mode: '" + name + "'");
		}
		else
		{
			for (uint32 i = 0; i < shapeModesCount; i++)
				if (name == shapeModeNames[i])
					terrainShapeFnc = shapeModeFunctions[i];
			if (!terrainShapeFnc)
			{
				CAGE_LOG_THROW(stringizer() + "shape mode: '" + name + "'");
				CAGE_THROW_ERROR(Exception, "unknown shape mode configuration");
			}
			CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using shape mode: '" + name + "'");
		}
	}
}

real terrainElevation(const vec3 &pos)
{
	CAGE_ASSERT(terrainElevationFnc != nullptr);
	return terrainElevationFnc(pos);
}

real terrainShape(const vec3 &pos)
{
	CAGE_ASSERT(terrainShapeFnc != nullptr);
	CAGE_ASSERT(terrainElevationFnc != nullptr);
	real base = terrainShapeFnc(pos);
	real elev = terrainElevationFnc(pos) * 10;
	real result = base - max(elev, 0);
	if (!valid(result))
		CAGE_THROW_ERROR(Exception, "invalid shape function value");
	return result;
}

void terrainApplyConfig()
{
	chooseShapeFunction();
	chooseElevationFunction();
}
