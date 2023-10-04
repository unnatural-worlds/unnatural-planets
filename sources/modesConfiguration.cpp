#include "math.h"
#include "sdf.h"
#include "tile.h"

#include <cage-core/config.h>
#include <cage-core/files.h>
#include <cage-core/noiseFunction.h>

namespace unnatural
{
	Real elevationNone(const Vec3 &);
	Real elevationSimple(const Vec3 &);
	Real elevationLegacy(const Vec3 &);
	Real elevationLakes(const Vec3 &);
	Real elevationIslands(const Vec3 &);
	Real elevationCraters(const Vec3 &);

	void coloringDefault(Tile &tile);
	void coloringBarren(Tile &tile);
	void coloringDebug(Tile &tile);

	namespace
	{
		ConfigString configShapeMode("unnatural-planets/shape/mode");
		const ConfigString configElevationMode("unnatural-planets/elevation/mode");
		const ConfigString configColoringMode("unnatural-planets/coloring/mode");
		const ConfigBool configPolesEnable("unnatural-planets/poles/enable");
		const ConfigBool configFlowersEnable("unnatural-planets/flowers/enable");

		using TerrainFunctor = Real (*)(const Vec3 &);
		TerrainFunctor terrainElevationFnc = 0;
		TerrainFunctor terrainShapeFnc = 0;

		using ColoringFunctor = void (*)(Tile &tile);
		ColoringFunctor coloringFnc = 0;

		void chooseShapeFunction()
		{
			static constexpr TerrainFunctor shapeModeFunctions[] = {
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
				&sdfFibers,
				&sdfH2O,
				&sdfH3O,
				&sdfH4O,
				&sdfTriangularPrism,
				&sdfHexagonalPrism,
				&sdfGear,
				&sdfMandelbulb,
				&sdfTwistedHexagonalPrism,
				&sdfBunny,
				&sdfMonkeyHead,
				&sdfDoubleTorus,
				&sdfTorusCross,
				&sdfBowl,
				&sdfInsideCube,
				&sdfAsteroid,
				&sdfPipe,
				&sdfTwistedPlane,
			};

			static constexpr uint32 shapeModesCount = sizeof(shapeModeFunctions) / sizeof(shapeModeFunctions[0]);

			static constexpr const char *const shapeModeNames[] = {
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
				"fibers",
				"h2o",
				"h3o",
				"h4o",
				"triangularprism",
				"hexagonalprism",
				"gear",
				"mandelbulb",
				"twistedhexagonalprism",
				"bunny",
				"monkeyhead",
				"doubletorus",
				"toruscross",
				"bowl",
				"insidecube",
				"asteroid",
				"pipe",
				"twistedplane",
			};

			static_assert(shapeModesCount == sizeof(shapeModeNames) / sizeof(shapeModeNames[0]), "number of functions and names must match");

			String name = configShapeMode;
			if (name == "random")
			{
				const uint32 i = randomRange(0u, shapeModesCount);
				terrainShapeFnc = shapeModeFunctions[i];
				configShapeMode = name = shapeModeNames[i];
				CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "randomly chosen shape mode: '" + name + "'");
			}
			else
			{
				for (uint32 i = 0; i < shapeModesCount; i++)
					if (name == shapeModeNames[i])
						terrainShapeFnc = shapeModeFunctions[i];
				if (!terrainShapeFnc)
				{
					CAGE_LOG_THROW(Stringizer() + "shape mode: '" + name + "'");
					CAGE_THROW_ERROR(Exception, "unknown shape mode configuration");
				}
				CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "using shape mode: '" + name + "'");
			}
		}

		void chooseElevationFunction()
		{
			constexpr TerrainFunctor elevationModeFunctions[] = {
				&elevationNone,
				&elevationSimple,
				&elevationLegacy,
				&elevationLakes,
				&elevationIslands,
				&elevationCraters,
			};

			constexpr uint32 elevationModesCount = sizeof(elevationModeFunctions) / sizeof(elevationModeFunctions[0]);

			constexpr const char *const elevationModeNames[] = {
				"none",
				"simple",
				"legacy",
				"lakes",
				"islands",
				"craters",
			};

			static_assert(elevationModesCount == sizeof(elevationModeNames) / sizeof(elevationModeNames[0]), "number of functions and names must match");

			for (uint32 i = 0; i < elevationModesCount; i++)
				if ((String)configElevationMode == elevationModeNames[i])
					terrainElevationFnc = elevationModeFunctions[i];
			if (!terrainElevationFnc)
			{
				CAGE_LOG_THROW(Stringizer() + "elevation mode: '" + (String)configElevationMode + "'");
				CAGE_THROW_ERROR(Exception, "unknown elevation mode configuration");
			}
			CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "using elevation mode: '" + (String)configElevationMode + "'");
		}

		void chooseColoringFunction()
		{
			constexpr ColoringFunctor tileModeFunctions[] = {
				&coloringDefault,
				&coloringBarren,
				&coloringDebug,
			};

			constexpr uint32 tileModesCount = sizeof(tileModeFunctions) / sizeof(tileModeFunctions[0]);

			constexpr const char *const tileModeNames[] = {
				"default",
				"barren",
				"debug",
			};

			static_assert(tileModesCount == sizeof(tileModeNames) / sizeof(tileModeNames[0]), "number of functions and names must match");

			for (uint32 i = 0; i < tileModesCount; i++)
				if ((String)configColoringMode == tileModeNames[i])
					coloringFnc = tileModeFunctions[i];
			if (!coloringFnc)
			{
				CAGE_LOG_THROW(Stringizer() + "coloring mode: '" + (String)configColoringMode + "'");
				CAGE_THROW_ERROR(Exception, "unknown coloring mode configuration");
			}
			CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "using coloring mode: '" + (String)configColoringMode + "'");
		}

		constexpr Real meshElevationRatio = 10;
	}

	Real terrainSdfElevation(const Vec3 &pos)
	{
		CAGE_ASSERT(terrainShapeFnc != nullptr);
		const Real result = terrainShapeFnc(pos) * meshElevationRatio;
		if (!valid(result))
			CAGE_THROW_ERROR(Exception, "invalid elevation sdf value");
		return result;
	}

	Real terrainSdfElevationRaw(const Vec3 &pos)
	{
		CAGE_ASSERT(terrainElevationFnc != nullptr);
		const Real result = terrainElevationFnc(pos);
		if (!valid(result))
			CAGE_THROW_ERROR(Exception, "invalid elevation raw sdf value");
		return result;
	}

	Real terrainSdfLand(const Vec3 &pos)
	{
		CAGE_ASSERT(terrainShapeFnc != nullptr);
		CAGE_ASSERT(terrainElevationFnc != nullptr);
		const Real base = terrainShapeFnc(pos);
		const Real elev = terrainElevationFnc(pos) / meshElevationRatio;
		const Real result = base - elev;
		if (!valid(result))
			CAGE_THROW_ERROR(Exception, "invalid land sdf value");
		return result;
	}

	Real terrainSdfWater(const Vec3 &pos)
	{
		CAGE_ASSERT(terrainShapeFnc != nullptr);
		const Real result = terrainShapeFnc(pos);
		if (!valid(result))
			CAGE_THROW_ERROR(Exception, "invalid water sdf value");
		return result;
	}

	Real terrainSdfNavigation(const Vec3 &pos)
	{
		CAGE_ASSERT(terrainShapeFnc != nullptr);
		CAGE_ASSERT(terrainElevationFnc != nullptr);
		const Real base = terrainShapeFnc(pos);
		const Real elev = terrainElevationFnc(pos) / meshElevationRatio;
		const Real result = base - max(elev, 0);
		if (!valid(result))
			CAGE_THROW_ERROR(Exception, "invalid navigation sdf value");
		return result;
	}

	void terrainPreseed()
	{
		{
			terrainSdfLand(Vec3());
			Tile tile;
			tile.position = Vec3(0, 1, 0);
			tile.normal = Vec3(0, 1, 0);
			tile.meshPurpose = MeshPurposeEnum::Land;
			terrainTile(tile);
		}
		{
			terrainSdfWater(Vec3());
			Tile tile;
			tile.position = Vec3(0, 1, 0);
			tile.normal = Vec3(0, 1, 0);
			tile.meshPurpose = MeshPurposeEnum::Water;
			terrainTile(tile);
		}
	}

	void terrainTile(Tile &tile)
	{
		CAGE_ASSERT(coloringFnc != nullptr);
		CAGE_ASSERT(isUnit(tile.normal));
		coloringFnc(tile);
	}

	void terrainApplyConfig()
	{
		chooseShapeFunction();
		chooseElevationFunction();
		chooseColoringFunction();
		CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable poles: " + !!configPolesEnable);
		CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable flowers: " + !!configFlowersEnable);
	}

	void writeConfigurationDescription(File *f)
	{
		f->writeLine(Stringizer() + "shape: " + (String)configShapeMode);
		f->writeLine(Stringizer() + "elevations: " + (String)configElevationMode);
		f->writeLine(Stringizer() + "coloring: " + (String)configColoringMode);
		f->writeLine(Stringizer() + "poles: " + (bool)configPolesEnable);
	}
}
