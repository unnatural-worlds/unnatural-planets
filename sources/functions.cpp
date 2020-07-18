#include "generator.h"
#include "functions.h"

#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>
#include <cage-core/ini.h>

namespace
{
	const uint32 globalSeed = (uint32)detail::getApplicationRandomGenerator().next();

	uint32 noiseSeed()
	{
		static uint32 offset = 0;
		offset++;
		return hash(globalSeed + offset);
	}

	template <class T>
	T rescale(const T &v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	real sharpEdge(real v, real p = 0.05)
	{
		return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
	}

	vec3 colorDeviation(const vec3 &color, real deviation = 0.05)
	{
		vec3 hsl = colorRgbToHsluv(color) + (randomChance3() - 0.5) * deviation;
		hsl[0] = (hsl[0] + 1) % 1;
		return colorHsluvToRgb(clamp(hsl, 0, 1));
	}

	vec3 normalDeviation(const vec3 &normal, real strength)
	{
		return normalize(normal + strength * randomDirection3());
	}

	bool isUnit(const vec3 &v)
	{
		return abs(length(v) - 1) < 1e-3;
	}

	vec3 anyPerpendicular(const vec3 &a)
	{
		CAGE_ASSERT(isUnit(a));
		vec3 b = vec3(1, 0, 0);
		if (abs(dot(a, b)) > 0.9)
			b = vec3(0, 1, 0);
		return normalize(cross(a, b));
	}

	//-----------
	// densities
	//-----------

	real densityPlane(const vec3 &pos, const plane &pln)
	{
		CAGE_ASSERT(pln.valid());
		vec3 c = pln.normal * pln.d;
		return -dot(pln.normal, pos - c);
	}

	real densityPlane(const vec3 &pos)
	{
		return densityPlane(pos, plane(vec3(), normalize(vec3(1))));
	}

	real densitySphere(const vec3 &pos, real radius)
	{
		return radius - length(pos);
	}

	real densitySphere(const vec3 &pos)
	{
		return densitySphere(pos, 100);
	}

	real densityTorus(const vec3 &pos, real major, real minor)
	{
		vec3 c = normalize(pos * vec3(1, 0, 1)) * major;
		return minor - distance(pos, c);
	}

	real densityTorus(const vec3 &pos)
	{
		return densityTorus(pos, 75, 25);
	}

	real densityCylinder(const vec3 &pos, real r, real h, real rounding)
	{
		vec2 d = abs(vec2(length(vec2(pos[0], pos[2])), pos[1])) - vec2(r, h);
		return -min(max(d[0], d[1]), 0) - length(max(d, 0)) + rounding;
	}

	real densityCylinder(const vec3 &pos)
	{
		return densityCylinder(pos, 80, 70, 5);
	}

	real densityBox(const vec3 &pos, const vec3 &size, real rounding)
	{
		vec3 p = abs(pos) - size;
		real box = length(max(p, 0)) + min(max(p[0], max(p[1], p[2])), 0) - rounding;
		return -box;
	}

	real densityBox(const vec3 &pos)
	{
		return densityBox(pos, vec3(100, 50, 50), 5);
	}

	real densityTetrahedron(const vec3 &pos, real size, real rounding)
	{
		const vec3 corners[4] = { vec3(1,1,1)*size, vec3(1,-1,-1)*size, vec3(-1,1,-1)*size, vec3(-1,-1,1)*size };
		const triangle tris[4] = {
			triangle(corners[0], corners[1], corners[2]),
			triangle(corners[0], corners[3], corners[1]),
			triangle(corners[0], corners[2], corners[3]),
			triangle(corners[1], corners[3], corners[2])
		};
		const auto &sideOfAPlane = [](const triangle &tri, const vec3 &pt) -> bool
		{
			plane pl(tri);
			return dot(pl.normal, pt) < -pl.d;
		};
		const bool insides[4] = {
			sideOfAPlane(tris[0], pos),
			sideOfAPlane(tris[1], pos),
			sideOfAPlane(tris[2], pos),
			sideOfAPlane(tris[3], pos)
		};
		const real lens[4] = {
			distance(tris[0], pos),
			distance(tris[1], pos),
			distance(tris[2], pos),
			distance(tris[3], pos)
		};
		if (insides[0] && insides[1] && insides[2] && insides[3])
		{
			real r = min(min(lens[0], lens[1]), min(lens[2], lens[3]));
			CAGE_ASSERT(r.valid() && r.finite());
			return r + rounding;
		}
		else
		{
			real r = real::Infinity();
			for (int i = 0; i < 4; i++)
			{
				if (!insides[i])
					r = min(r, lens[i]);
			}
			CAGE_ASSERT(r.valid() && r.finite());
			return -r + rounding;
		}
	}

	real densityTetrahedron(const vec3 &pos)
	{
		return densityTetrahedron(pos, 100, 5);
	}

	real densityOctahedron(const vec3 &pos, real size, real rounding)
	{
		vec3 p = abs(pos);
		real m = p[0] + p[1] + p[2] - size;
		vec3 q;
		if (3.0 * p[0] < m)
			q = p;
		else if (3.0 * p[1] < m)
			q = vec3(p[1], p[2], p[0]);
		else if (3.0 * p[2] < m)
			q = vec3(p[2], p[0], p[1]);
		else
			return m * -0.57735027 + rounding;
		real k = clamp(0.5 * (q[2] - q[1] + size), 0.0, size);
		return -length(vec3(q[0], q[1] - size + k, q[2] - k)) + rounding;
	}

	real densityOctahedron(const vec3 &pos)
	{
		return densityOctahedron(pos, 100, 5);
	}

	real densityPretzel(const vec3 &pos)
	{
		vec3 c = normalize(pos * vec3(1, 0, 1));
		rads yaw = atan2(c[0], c[2]);
		rads pitch = atan2(pos[1], 70 - length(vec2(pos[0], pos[2])));
		rads ang = yaw + pitch;
		real t = length(vec2(sin(ang) * 25, cos(ang) * 5));
		real l = distance(pos, c * 70);
		return t - l;
	}

	real densityMobiusStrip(const vec3 &pos, real radius, real majorAxis, real minorAxis, real rounding)
	{
		const auto &sdAlignedRect = [](const vec2 &point, const vec2 &halfSizes) -> real
		{
			vec2 d = abs(point) - halfSizes;
			return length(max(d, 0)) + min(max(d[0], d[1]), 0);
		};

		const auto &sdRotatedRect = [&](const vec2 &point, const vec2 &halfSizes, rads rotation) -> real
		{
			// rotate the point instead of the rectangle
			rads a = atan2(point[0], point[1]);
			a += rotation;
			vec2 p = vec2(cos(a), sin(a)) * length(point);
			return sdAlignedRect(p, halfSizes);
		};

		rads planeRotation = atan2(pos[0], pos[2]);
		vec2 proj = vec2(dot(vec2(pos[0], pos[2]), normalize(vec2(pos[0], pos[2]))), pos[1]);
		return -sdRotatedRect(proj + vec2(-radius, 0), vec2(majorAxis, minorAxis), planeRotation / 2) + rounding;
	}

	real densityMobiusStrip(const vec3 &pos)
	{
		return densityMobiusStrip(pos, 70, 30, 2, 5);
	}

	real densityMolecule(const vec3 &pos)
	{
		const auto &sdGyroid = [](vec3 p, real scale, real thickness, real bias)
		{
			p *= scale;
			vec3 a = vec3(sin(rads(p[0])), sin(rads(p[1])), sin(rads(p[2])));
			vec3 b = vec3(cos(rads(p[2])), cos(rads(p[0])), cos(rads(p[1])));
			return abs(dot(a, b) + bias) / scale - thickness;
		};

		static const real scale = 0.02;
		real box = -densityBox(pos * scale, vec3(1.5), 0.05);
		static const vec3 offset = randomRange3(-1000, 1000);
		const vec3 p = (pos + offset) * scale;
		real g1 = sdGyroid(p, 3.23, 0.03, 1.4);
		real g2 = sdGyroid(p, 5.78, 0.05, 0.3);
		real g3 = sdGyroid(p, 12.21, 0.02, 0.1);
		real g4 = sdGyroid(p, 17.13, 0.03, 0.3);
		real g = g1 - g2*0.37 + g3*0.2 + g4*0.11;
		return -max(box, g * 0.7) / scale;
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

	static const vec3 deepWaterColor = vec3(54, 54, 97) / 255;
	static const vec3 shallowWaterColor = vec3(26, 102, 125) / 255;

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

	bool useTerrainElevation = true;

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
		a += 0.1; // slightly prefer terrain over ocean
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

	bool useTerrainPoles = false;

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
	}
}

stringizer &operator + (stringizer &str, const BiomeEnum &other)
{
	switch (other)
	{
	case BiomeEnum::Bare: str + "Bare"; break;
	case BiomeEnum::Tundra: str + "Tundra"; break;
	case BiomeEnum::Taiga: str + "Taiga"; break;
	case BiomeEnum::Shrubland: str + "Shrubland"; break;
	case BiomeEnum::Grassland: str + "Grassland"; break;
	case BiomeEnum::TemperateSeasonalForest: str + "TemperateSeasonalForest"; break;
	case BiomeEnum::TemperateRainForest: str + "TemperateRainForest"; break;
	case BiomeEnum::Desert: str + "Desert"; break;
	case BiomeEnum::Savanna: str + "Savanna"; break;
	case BiomeEnum::TropicalSeasonalForest: str + "TropicalSeasonalForest"; break;
	case BiomeEnum::TropicalRainForest: str + "TropicalRainForest"; break;
	case BiomeEnum::_Ocean: str + "Ocean"; break;
	default: str + "<unknown>"; break;
	}
	return str;
}

stringizer &operator + (stringizer &str, const TerrainTypeEnum &other)
{
	switch (other)
	{
	case TerrainTypeEnum::Road: str + "Road"; break;
	case TerrainTypeEnum::Slow: str + "Slow"; break;
	case TerrainTypeEnum::Fast: str + "Fast"; break;
	case TerrainTypeEnum::SteepSlope: str + "SteepSlope"; break;
	case TerrainTypeEnum::ShallowWater: str + "ShallowWater"; break;
	case TerrainTypeEnum::DeepWater: str + "DeepWater"; break;
	default: str + "<unknown>"; break;
	}
	return str;
}

real functionDensity(const vec3 &pos)
{
	CAGE_ASSERT(baseShapeDensity != nullptr);
	real base = baseShapeDensity(pos);
	real elev = terrainElevation(pos);
	return base + max(elev, 0);
}

void functionTileProperties(const vec3 &pos, const vec3 &normal, BiomeEnum &biome, TerrainTypeEnum &terrainType, real &elevation, real &temperature, real &precipitation)
{
	vec3 albedo;
	vec2 special;
	real height;
	terrain(pos, normal, biome, terrainType, elevation, temperature, precipitation, albedo, special, height);
}

void functionMaterial(const vec3 &pos, const vec3 &normal, vec3 &albedo, vec2 &special, real &height)
{
	BiomeEnum biome;
	TerrainTypeEnum terrainType;
	real elevation;
	real temperature;
	real precipitation;
	terrain(pos, normal, biome, terrainType, elevation, temperature, precipitation, albedo, special, height);
}

void functionsConfigure(const Holder<Ini> &cmd)
{
	constexpr BaseShapeDensity baseShapeFunctions[] = {
		&densityPlane,
		&densitySphere,
		&densityTorus,
		&densityCylinder,
		&densityBox,
		&densityTetrahedron,
		&densityOctahedron,
		&densityPretzel,
		&densityMobiusStrip,
		&densityMolecule,
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
		"pretzel",
		"mobiusStrip",
		"molecule",
	};

	static_assert(baseShapesCount == sizeof(baseShapeNames) / sizeof(baseShapeNames[0]), "number of functions and names must match");

	{ // base shape
		string name = cmd->cmdString('s', "shape", "random");
		if (name == "random")
		{
			const uint32 i = randomRange(0u, baseShapesCount);
			baseShapeDensity = baseShapeFunctions[i];
			CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "randomly chosen base shape: '" + baseShapeNames[i] + "'");
		}
		else
		{
			for (uint32 i = 0; i < baseShapesCount; i++)
			{
				if (name == baseShapeNames[i])
				{
					baseShapeDensity = baseShapeFunctions[i];
					CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using base shape: '" + baseShapeNames[i] + "'");
				}
			}
		}
		if (!baseShapeDensity)
		{
			CAGE_LOG(SeverityEnum::Note, "exception", stringizer() + "base shape: '" + name + "'");
			CAGE_THROW_ERROR(Exception, "unknown base shape configuration");
		}
	}

	{ // poles
		useTerrainPoles = baseShapeDensity == baseShapeFunctions[1];
		useTerrainPoles = cmd->cmdBool('p', "poles", useTerrainPoles);
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using poles: " + useTerrainPoles);
	}

	{ // elevation
		useTerrainElevation = cmd->cmdBool('e', "elevation", useTerrainElevation);
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using terrain elevation: " + useTerrainElevation);
	}

	preseedTerrainFunctions();
}

