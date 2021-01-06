#include "terrain.h"

stringizer &operator + (stringizer &str, const TerrainBiomeEnum &other)
{
	switch (other)
	{
	case TerrainBiomeEnum::Bare: str + "Bare"; break;
	case TerrainBiomeEnum::Tundra: str + "Tundra"; break;
	case TerrainBiomeEnum::Taiga: str + "Taiga"; break;
	case TerrainBiomeEnum::Shrubland: str + "Shrubland"; break;
	case TerrainBiomeEnum::Grassland: str + "Grassland"; break;
	case TerrainBiomeEnum::TemperateSeasonalForest: str + "TemperateSeasonalForest"; break;
	case TerrainBiomeEnum::TemperateRainForest: str + "TemperateRainForest"; break;
	case TerrainBiomeEnum::Desert: str + "Desert"; break;
	case TerrainBiomeEnum::Savanna: str + "Savanna"; break;
	case TerrainBiomeEnum::TropicalSeasonalForest: str + "TropicalSeasonalForest"; break;
	case TerrainBiomeEnum::TropicalRainForest: str + "TropicalRainForest"; break;
	case TerrainBiomeEnum::Ocean: str + "Ocean"; break;
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
