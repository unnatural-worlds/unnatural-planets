#include <cage-core/color.h>

#include "generator.h"

real sharpEdge(real v, real p)
{
	return rescale(clamp(v, 0.5 - p, 0.5 + p), 0.5 - p, 0.5 + p, 0, 1);
}

vec3 colorDeviation(const vec3 &color, real deviation)
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
