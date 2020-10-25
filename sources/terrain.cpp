#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>
#include <cage-core/config.h>

#include "generator.h"
#include "sdf.h"

namespace
{
	ConfigString baseShapeName("unnatural-planets/planet/shape");
	ConfigBool useTerrainPoles("unnatural-planets/planet/poles");
	ConfigBool useTerrainElevation("unnatural-planets/planet/elevation");

	const uint32 globalSeed = (uint32)detail::getApplicationRandomGenerator().next();

	uint32 noiseSeed()
	{
		static uint32 offset = 0;
		offset++;
		return hash(globalSeed + offset);
	}

	typedef real (*BaseShapeDensity)(const vec3 &);
	BaseShapeDensity baseShapeDensity = 0;

	//--------
	// biomes
	//--------

	BiomeEnum biome(real elev, rads slope, real temp, real precp)
	{
		if (temp > 20)
		{
			if (precp < 40)
				return BiomeEnum::Desert;
			if (precp < 130)
				return BiomeEnum::Savanna;
			if (precp < 230)
				return BiomeEnum::TropicalSeasonalForest;
			return BiomeEnum::TropicalRainForest;
		}
		if (temp > 5)
		{
			if (precp < 30)
				return BiomeEnum::Shrubland;
			if (precp < 100)
				return BiomeEnum::Grassland;
			if (precp < 200)
				return BiomeEnum::TemperateSeasonalForest;
			return BiomeEnum::TemperateRainForest;
		}
		if (temp > -5)
		{
			if (precp < 30)
				return BiomeEnum::Bare;
			return BiomeEnum::Taiga;
		}
		if (precp < 10)
			return BiomeEnum::Bare;
		return BiomeEnum::Tundra;
	}

	vec3 biomeColor(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Bare: return vec3(187) / 255;
		case BiomeEnum::Tundra: return vec3(221, 221, 187) / 255;
		case BiomeEnum::Taiga: return vec3(204, 212, 187) / 255;
		case BiomeEnum::Shrubland: return vec3(196, 204, 187) / 255;
		case BiomeEnum::Grassland: return vec3(196, 212, 170) / 255;
		case BiomeEnum::TemperateSeasonalForest: return vec3(180, 201, 169) / 255;
		case BiomeEnum::TemperateRainForest: return vec3(164, 196, 168) / 255;
		case BiomeEnum::Desert: return vec3(172, 159, 139) / 255;
		case BiomeEnum::Savanna: return vec3(228, 232, 202) / 255;
		case BiomeEnum::TropicalSeasonalForest: return vec3(169, 204, 164) / 255;
		case BiomeEnum::TropicalRainForest: return vec3(156, 187, 169) / 255;
		default: return vec3::Nan();
		};
	}

	constexpr vec3 deepWaterColor = vec3(54, 54, 97) / 255;
	constexpr vec3 shallowWaterColor = vec3(26, 102, 125) / 255;

	real biomeRoughness(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Bare: return 0.9;
		case BiomeEnum::Tundra: return 0.7;
		case BiomeEnum::Taiga: return 0.7;
		case BiomeEnum::Shrubland: return 0.8;
		case BiomeEnum::Grassland: return 0.7;
		case BiomeEnum::TemperateSeasonalForest: return 0.7;
		case BiomeEnum::TemperateRainForest: return 0.7;
		case BiomeEnum::Desert: return 0.7;
		case BiomeEnum::Savanna: return 0.7;
		case BiomeEnum::TropicalSeasonalForest: return 0.7;
		case BiomeEnum::TropicalRainForest: return 0.7;
		default: return real::Nan();
		};
	}

	real biomeHeight(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Bare: return 0.2;
		case BiomeEnum::Tundra: return 0.8;
		case BiomeEnum::Taiga: return 0.7;
		case BiomeEnum::Shrubland: return 0.6;
		case BiomeEnum::Grassland: return 0.5;
		case BiomeEnum::TemperateSeasonalForest: return 0.7;
		case BiomeEnum::TemperateRainForest: return 0.8;
		case BiomeEnum::Desert: return 0.3;
		case BiomeEnum::Savanna: return 0.4;
		case BiomeEnum::TropicalSeasonalForest: return 0.7;
		case BiomeEnum::TropicalRainForest: return 0.8;
		default: return real::Nan();
		};
	}

	real biomeDiversity(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Bare: return 0.04;
		case BiomeEnum::Tundra: return 0.03;
		case BiomeEnum::Taiga: return 0.03;
		case BiomeEnum::Shrubland: return 0.04;
		case BiomeEnum::Grassland: return 0.06;
		case BiomeEnum::TemperateSeasonalForest: return 0.07;
		case BiomeEnum::TemperateRainForest: return 0.09;
		case BiomeEnum::Desert: return 0.04;
		case BiomeEnum::Savanna: return 0.06;
		case BiomeEnum::TropicalSeasonalForest: return 0.07;
		case BiomeEnum::TropicalRainForest: return 0.09;
		default: return real::Nan();
		};
	}

	TerrainTypeEnum biomeTerrainType(BiomeEnum b)
	{
		switch (b)
		{
		case BiomeEnum::Shrubland:
		case BiomeEnum::Grassland:
		case BiomeEnum::Savanna:
		case BiomeEnum::TemperateSeasonalForest:
		case BiomeEnum::TropicalSeasonalForest:
			return TerrainTypeEnum::Fast;
		case BiomeEnum::Tundra:
		case BiomeEnum::Taiga:
		case BiomeEnum::Desert:
		case BiomeEnum::TemperateRainForest:
		case BiomeEnum::TropicalRainForest:
			return TerrainTypeEnum::Slow;
		case BiomeEnum::Bare:
		default:
			return TerrainTypeEnum::Road;
		}
	}

	//---------
	// terrain
	//---------

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

	real terrainPrecipitation(const vec3 &pos, real elev)
	{
		static const Holder<NoiseFunction> precpNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = precpNoise->evaluate(pos * 0.015) * 0.5 + 0.5;
		p = clamp(p, 0, 1);
		p = smootherstep(p);
		p = smootherstep(p);
		p = smootherstep(p);
		p = pow(p, 1.6);
		p += max(1 - abs(elev), 0) * 0.15; // more water close to oceans
		return p * 400;
	}

	real terrainTemperature(const vec3 &pos, real elev)
	{
		static const Holder<NoiseFunction> tempNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real t = tempNoise->evaluate(pos * 0.015) * 0.5 + 0.5;
		t = clamp(t, 0, 1);
		t = smootherstep(t);
		t = smootherstep(t);
		t = t * 2 - 1;
		return 30 + t * 5 - abs(elev) * 2.8;
	}

	void terrainPoles(const vec3 &pos, real &temp)
	{
		static const Holder<NoiseFunction> polarNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (!useTerrainPoles)
			return;
		real polar = abs(atan(pos[1] / length(vec2(pos[0], pos[2]))).value) / real::Pi() * 2;
		polar = pow(polar, 1.7);
		polar += polarNoise->evaluate(pos * 0.07) * 0.1;
		temp += 15 - polar * 80;
	}

	rads terrainSlope(const vec3 &pos, const vec3 &normal, real radius = 0.05)
	{
		vec3 a = anyPerpendicular(normal);
		vec3 b = cross(normal, a);
		a *= radius;
		b *= radius;
		vec3 c = (a + b) / sqrt(2);
		vec3 d = (a - b) / sqrt(2);
		real elevs[8] = {
			terrainElevation(pos + a),
			terrainElevation(pos + b),
			terrainElevation(pos - a),
			terrainElevation(pos - b),
			terrainElevation(pos + c),
			terrainElevation(pos + d),
			terrainElevation(pos - c),
			terrainElevation(pos - d),
		};
		real difs[4] = {
			abs(elevs[2] - elevs[0]),
			abs(elevs[3] - elevs[1]),
			abs(elevs[6] - elevs[4]),
			abs(elevs[7] - elevs[5]),
		};
		real md = max(max(difs[0], difs[1]), max(difs[2], difs[3]));
		return atan(md / radius);
	}

	void oceanOverrides(const vec3 &pos, const vec3 &normal, real elev, BiomeEnum &biom, TerrainTypeEnum &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		if (elev > 0)
			return;
		CAGE_ASSERT(elev <= 0);
		real shallow = -0.5 / (elev - 0.5);
		shallow = clamp(shallow, 0, 1);
		shallow = smoothstep(shallow);
		terrainType = shallow > 0.5 ? TerrainTypeEnum::ShallowWater : TerrainTypeEnum::DeepWater;
		albedo = interpolate(deepWaterColor, shallowWaterColor, shallow);
		special[0] = 0.3;
		biom = BiomeEnum::_Ocean;

		{ // waves
			static const Holder<NoiseFunction> xNoise = []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> yNoise = []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			static const Holder<NoiseFunction> zNoise = []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 4;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}();
			vec3 s = pos * 0.01;
			real x = xNoise->evaluate(s);
			real y = yNoise->evaluate(s);
			real z = zNoise->evaluate(s);
			vec3 dir = normalize(vec3(x, y, z));
			//CAGE_ASSERT(isUnit(dir));
			//dir = normalize(dir - dot(dir, normal) * normal);
			CAGE_ASSERT(isUnit(dir));
			real dist = dot(dir, pos) * length(pos);
			real wave = sin(rads(dist * 0.05));
			height = wave * 0.05 + 0.05;
		}
	}

	void iceOverrides(const vec3 &pos, real elev, real temp, TerrainTypeEnum &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Natural;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> scaleNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (elev > -2 || temp > -5)
			return;
		real scale = 0.5 + scaleNoise->evaluate(pos * 0.3) * 0.02;
		real crack = cracksNoise->evaluate(pos * scale);
		crack = pow(crack, 0.3);
		terrainType = TerrainTypeEnum::Slow;
		albedo = vec3(61, 81, 82) / 255 + crack * 0.3;
		special[0] = 0.15 + (1 - crack) * 0.6;
		height = 0.2 + crack * 0.2;
	}

	void slopeOverrides(const vec3 &pos, real elev, rads slope, TerrainTypeEnum &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const uint32 seed = noiseSeed();
		static const Holder<NoiseFunction> cracksNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Natural;
			cfg.operation = NoiseOperationEnum::Subtract;
			cfg.seed = seed;
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> typeNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cellular;
			cfg.distance = NoiseDistanceEnum::Natural;
			cfg.operation = NoiseOperationEnum::NoiseLookup;
			cfg.seed = seed;
			return newNoiseFunction(cfg, []() {
				NoiseFunctionCreateConfig cfg;
				cfg.type = NoiseTypeEnum::Value;
				cfg.octaves = 2;
				cfg.seed = noiseSeed();
				return newNoiseFunction(cfg);
			}());
		}();
		if (elev < 0)
			return;
		real factor = rescale(slope.value, 0, real::Pi() * 0.5, -1, 2);
		factor = sharpEdge(factor);
		real scale = 3;
		real crack = cracksNoise->evaluate(pos * scale) * 0.5 + 0.5;
		crack = pow(crack, 0.8);
		real type = typeNoise->evaluate(pos * scale) * 0.5 + 0.5;
		if (factor > 0.5)
			terrainType = TerrainTypeEnum::SteepSlope;
		vec3 rocksAlbedo = vec3((type * 0.6 + 0.2) * crack);
		albedo = interpolate(albedo, rocksAlbedo, factor);
		real rocksRoughness = interpolate(0.9, 0.3 + type * 0.6, crack);
		special[0] = interpolate(special[0], rocksRoughness, factor);
		real rocksHeight = 0.35 + crack * 0.3;
		height = interpolate(height, rocksHeight, factor);
	}

	void grassOverrides(const vec3 &pos, real elev, rads slope, real temp, real precp, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> threadsNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::RigidMulti;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		static const Holder<NoiseFunction> perturbNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (elev < 0)
			return;
		real thriving = 0.9;
		thriving *= clamp(rescale(slope.value, 0, real::Pi() / 2, 1.5, -0.5), 0, 1);
		thriving *= 2000 / (pow(abs(temp - 20), 4) + 2000);
		thriving *= pow(precp, 0.5) / (abs(precp - 70) + 10);
		real threshold = thresholdNoise->evaluate(pos * 1.5) * 0.5 + 0.5;
		real factor = sharpEdge(thriving - threshold + 0.5, 0.15);
		real blend = threadsNoise->evaluate(pos * (2 + perturbNoise->evaluate(pos) * 0.1)) * 0.5 + 0.5;
		vec3 grassAlbedo = interpolate(vec3(128, 152, 74), vec3(72, 106, 39), blend) / 255;
		albedo = interpolate(albedo, grassAlbedo, factor);
		special[0] = interpolate(special[0], 0.5, factor);
		height += (0.03 + blend * 0.05) * factor;
	}

	void snowOverrides(const vec3 &pos, real elev, rads slope, real temp, real precp, TerrainTypeEnum &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> thresholdNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if ((elev < 0 || temp > 0) && (elev > -2 || temp > -5))
			return;
		if (precp < 50 || terrainType == TerrainTypeEnum::SteepSlope)
			return;
		real factor = (thresholdNoise->evaluate(pos) * 0.5 + 0.5) * 0.6 + 0.4;
		terrainType = TerrainTypeEnum::Slow;
		albedo = interpolate(albedo, vec3(248) / 255, factor);
		special[0] = interpolate(special[0], 0.7, factor);
		height += 0.3 * factor;
	}

	void beachOverrides(const vec3 &pos, real elev, TerrainTypeEnum &terrainType, vec3 &albedo, vec2 &special, real &height)
	{
		static const Holder<NoiseFunction> solidNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.octaves = 4;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		if (elev < 0 || elev > 0.3)
			return;
		real solid = elev / 0.3;
		solid += solidNoise->evaluate(pos * 1.5) * 0.3;
		solid = clamp(solid, 0, 1);
		terrainType = TerrainTypeEnum::ShallowWater;
		albedo = interpolate(shallowWaterColor, albedo, solid);
		special[0] = interpolate(0.3, special[0], solid);
		height = solid * 0.2;
	}

	void terrain(const vec3 &pos, const vec3 &normal, BiomeEnum &biom, TerrainTypeEnum &terrainType, real &elev, real &temp, real &precp, vec3 &albedo, vec2 &special, real &height)
	{
		CAGE_ASSERT(isUnit(normal));
		elev = terrainElevation(pos);
		rads slope = terrainSlope(pos, normal);
		temp = terrainTemperature(pos, elev);
		terrainPoles(pos, temp);
		precp = terrainPrecipitation(pos, elev);
		biom = biome(elev, slope, temp, precp);
		terrainType = biomeTerrainType(biom);
		real diversity = biomeDiversity(biom);
		albedo = biomeColor(biom);
		special = vec2(biomeRoughness(biom), 0);
		height = biomeHeight(biom);
		oceanOverrides(pos, normal, elev, biom, terrainType, albedo, special, height);
		iceOverrides(pos, elev, temp, terrainType, albedo, special, height);
		slopeOverrides(pos, elev, slope, terrainType, albedo, special, height);
		//grassOverrides(pos, elev, slope, temp, precp, albedo, special, height);
		snowOverrides(pos, elev, slope, temp, precp, terrainType, albedo, special, height);
		beachOverrides(pos, elev, terrainType, albedo, special, height);
		albedo = saturate(albedo);
		albedo = colorDeviation(albedo, diversity);
		special[0] = special[0] + (randomChance() - 0.5) * diversity * 2;
		albedo = saturate(albedo);
		special = saturate(special);
		height = saturate(height);
	}

	real terrainFertility(const vec3 &pos)
	{
		static const Holder<NoiseFunction> fertNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = fertNoise->evaluate(pos * 0.035) * 2;
		p = clamp(p, -1, 1);
		return p;
	}

	real terrainNationality(const vec3 &pos)
	{
		static const Holder<NoiseFunction> natNoise = []() {
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.octaves = 4;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();
		real p = natNoise->evaluate(pos * 0.015) * 5;
		p = clamp(p, -1, 1);
		return p;
	}

	constexpr real globalPositionScale = 0.1;
}

real functionDensity(const vec3 &pos)
{
	CAGE_ASSERT(baseShapeDensity != nullptr);
	real base = baseShapeDensity(pos);
	real elev = terrainElevation(pos * globalPositionScale) / globalPositionScale;
	real result = base + max(elev, 0);
	if (!valid(result))
		CAGE_THROW_ERROR(Exception, "invalid density function value");
	return result;
}

void functionTileProperties(const vec3 &pos, const vec3 &normal, BiomeEnum &biome, TerrainTypeEnum &terrainType, real &elevation, real &temperature, real &precipitation)
{
	vec3 albedo;
	vec2 special;
	real height;
	terrain(pos * globalPositionScale, normal, biome, terrainType, elevation, temperature, precipitation, albedo, special, height);
}

void functionMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
{
	BiomeEnum biome;
	TerrainTypeEnum terrainType;
	real elevation;
	real temperature;
	real precipitation;
	terrain(pos * globalPositionScale, normal, biome, terrainType, elevation, temperature, precipitation, albedo, special, height);
}

void functionAuxiliaryProperties(const vec3 &pos, real &nationality, real &fertility)
{
	nationality = terrainNationality(pos * globalPositionScale);
	fertility = terrainFertility(pos * globalPositionScale);
}

void updateBaseShapeFunctionPointer()
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
		CAGE_LOG(SeverityEnum::Note, "exception", stringizer() + "base shape: '" + name + "'");
		CAGE_THROW_ERROR(Exception, "unknown base shape configuration");
	}
}

void preseedTerrainFunctions()
{
	functionDensity(vec3());
	BiomeEnum biom;
	TerrainTypeEnum terrainType;
	real elev;
	real temp;
	real precp;
	vec3 albedo;
	vec2 special;
	real height;
	terrain(vec3(), vec3(0, 1, 0), biom, terrainType, elev, temp, precp, albedo, special, height);
	real nationality;
	real fertility;
	functionAuxiliaryProperties(vec3(), nationality, fertility);
}
