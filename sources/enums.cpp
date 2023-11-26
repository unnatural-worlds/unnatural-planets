#include "planets.h"

namespace unnatural
{
	Stringizer &operator+(Stringizer &str, const TerrainBiomeEnum &other)
	{
		switch (other)
		{
			case TerrainBiomeEnum::Bare:
				str + "bare";
				break;
			case TerrainBiomeEnum::Tundra:
				str + "tundra";
				break;
			case TerrainBiomeEnum::Taiga:
				str + "taiga";
				break;
			case TerrainBiomeEnum::Shrubland:
				str + "shrubland";
				break;
			case TerrainBiomeEnum::Grassland:
				str + "grassland";
				break;
			case TerrainBiomeEnum::TemperateSeasonalForest:
				str + "temperate seasonal forest";
				break;
			case TerrainBiomeEnum::TemperateRainForest:
				str + "temperate rain forest";
				break;
			case TerrainBiomeEnum::Desert:
				str + "desert";
				break;
			case TerrainBiomeEnum::Savanna:
				str + "savanna";
				break;
			case TerrainBiomeEnum::TropicalSeasonalForest:
				str + "tropical seasonal forest";
				break;
			case TerrainBiomeEnum::TropicalRainForest:
				str + "tropical rain forest";
				break;
			case TerrainBiomeEnum::Water:
				str + "water";
				break;
			default:
				str + "<unknown>";
				break;
		}
		return str;
	}

	Stringizer &operator+(Stringizer &str, const TerrainTypeEnum &other)
	{
		switch (other)
		{
			case TerrainTypeEnum::Road:
				str + "road";
				break;
			case TerrainTypeEnum::Flat:
				str + "flat";
				break;
			case TerrainTypeEnum::Rough:
				str + "rough";
				break;
			case TerrainTypeEnum::Cliffs:
				str + "cliffs";
				break;
			case TerrainTypeEnum::ShallowWater:
				str + "shallow water";
				break;
			case TerrainTypeEnum::DeepWater:
				str + "deep water";
				break;
			default:
				str + "<unknown>";
				break;
		}
		return str;
	}
}
