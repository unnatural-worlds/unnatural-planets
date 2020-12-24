#include <cage-core/noiseFunction.h>
#include <cage-core/config.h>

#include "terrain.h"
#include "sdf.h"
#include "math.h"

namespace
{
	ConfigString baseShapeName("unnatural-planets/planet/shape");
	ConfigBool useTerrainElevation("unnatural-planets/planet/elevation");

	typedef real (*BaseShapeDensity)(const vec3 &);
	BaseShapeDensity baseShapeDensity = 0;

	constexpr real globalPositionScale = 0.1;
}

real terrainElevation(const vec3 &pos)
{
	static const Holder<NoiseFunction> scaleNoise = []() {
		NoiseFunctionCreateConfig cfg;
		cfg.type = NoiseTypeEnum::Value;
		cfg.octaves = 4;
		cfg.seed = noiseSeed();
		return newNoiseFunction(cfg);
	}();
	static const Holder<NoiseFunction> elevNoise = []() {
		NoiseFunctionCreateConfig cfg;
		cfg.type = NoiseTypeEnum::Value;
		cfg.octaves = 4;
		cfg.seed = noiseSeed();
		return newNoiseFunction(cfg);
	}();

	if (!useTerrainElevation)
		return 1;

	real scale = scaleNoise->evaluate(pos * 0.005) * 0.005 + 0.015;
	real a = elevNoise->evaluate(pos * scale);
	a += 0.11; // slightly prefer terrain over ocean
	if (a < 0)
		a = -pow(-a, 0.85);
	else
		a = pow(a, 1.7);
	return a * 25;
}

real terrainDensity(const vec3 &pos)
{
	CAGE_ASSERT(baseShapeDensity != nullptr);
	real base = baseShapeDensity(pos);
	real elev = terrainElevation(pos * globalPositionScale) / globalPositionScale;
	real result = base + max(elev, 0);
	if (!valid(result))
		CAGE_THROW_ERROR(Exception, "invalid density function value");
	return result;
}

void terrainApplyConfig()
{
	constexpr BaseShapeDensity baseShapeFunctions[] = {
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

	constexpr uint32 baseShapesCount = sizeof(baseShapeFunctions) / sizeof(baseShapeFunctions[0]);

	constexpr const char *const baseShapeNames[] = {
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

	static_assert(baseShapesCount == sizeof(baseShapeNames) / sizeof(baseShapeNames[0]), "number of functions and names must match");

	string name = baseShapeName;
	if (name == "random")
	{
		const uint32 i = randomRange(0u, baseShapesCount);
		baseShapeDensity = baseShapeFunctions[i];
		baseShapeName = name = baseShapeNames[i];
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "randomly chosen base shape: '" + name + "'");
	}
	else
	{
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using base shape: '" + name + "'");
		for (uint32 i = 0; i < baseShapesCount; i++)
			if (name == baseShapeNames[i])
				baseShapeDensity = baseShapeFunctions[i];
	}
	if (!baseShapeDensity)
	{
		CAGE_LOG_THROW(stringizer() + "base shape: '" + name + "'");
		CAGE_THROW_ERROR(Exception, "unknown base shape configuration");
	}
}
