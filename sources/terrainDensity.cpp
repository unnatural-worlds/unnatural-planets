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
	TerrainFunctor terrainDensityFnc = 0;

	real elevationNone(const vec3 &)
	{
		return 1;
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
		return 1; // todo
	}

	real elevationEarth(const vec3 &pos)
	{
		return 1; // todo
	}

	void chooseElevationFunction()
	{
		constexpr TerrainFunctor elevationModeFunctions[] = {
			&elevationNone,
			&elevationLegacy,
			&elevationTerraces,
			&elevationEarth,
		};

		constexpr uint32 elevationModesCount = sizeof(elevationModeFunctions) / sizeof(elevationModeFunctions[0]);

		constexpr const char *const elevationModeNames[] = {
			"none",
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

	void chooseDensityFunction()
	{
		constexpr TerrainFunctor shapeModeFunctions[] = {
			&sdfPlane,
			&sdfSphere,
			&sdfTorus,
			&sdfCylinder,
			&sdfBox,
			&sdfTetrahedron,
			&sdfOctahedron,
			&sdfKnot,
			&sdfPretzel,
			&sdfMobiusStrip,
			&sdfMolecule,
			&sdfH2O,
			&sdfH3O,
			&sdfH4O,
		};

		constexpr uint32 shapeModesCount = sizeof(shapeModeFunctions) / sizeof(shapeModeFunctions[0]);

		constexpr const char *const shapeModeNames[] = {
			"plane",
			"sphere",
			"torus",
			"cylinder",
			"box",
			"tetrahedron",
			"octahedron",
			"knot",
			"pretzel",
			"mobiusStrip",
			"molecule",
			"h2o",
			"h3o",
			"h4o",
		};

		static_assert(shapeModesCount == sizeof(shapeModeNames) / sizeof(shapeModeNames[0]), "number of functions and names must match");

		string name = configShapeMode;
		if (name == "random")
		{
			const uint32 i = randomRange(0u, shapeModesCount);
			terrainDensityFnc = shapeModeFunctions[i];
			configShapeMode = name = shapeModeNames[i];
			CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "randomly chosen shape mode: '" + name + "'");
		}
		else
		{
			for (uint32 i = 0; i < shapeModesCount; i++)
				if (name == shapeModeNames[i])
					terrainDensityFnc = shapeModeFunctions[i];
			if (!terrainDensityFnc)
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

real terrainDensity(const vec3 &pos)
{
	CAGE_ASSERT(terrainDensityFnc != nullptr);
	real base = terrainDensityFnc(pos);
	real elev = terrainElevation(pos) * 10;
	real result = base + max(elev, 0);
	if (!valid(result))
		CAGE_THROW_ERROR(Exception, "invalid density function value");
	return result;
}

void terrainApplyConfig()
{
	chooseDensityFunction();
	chooseElevationFunction();
}
