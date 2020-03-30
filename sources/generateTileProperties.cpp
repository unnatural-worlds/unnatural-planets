#include "generator.h"
#include "functions.h"

namespace
{
	string statistics(const string &name, uint32 current, uint32 total)
	{
		string c = stringizer() + current;
		string r = stringizer() + "(" + 100 * real(current) / total + ")";
		return stringizer() + name.fill(28) + c.reverse().fill(6).reverse() + r.reverse().fill(12).reverse();
	}
}

std::vector<uint8> generateTileProperties(const Holder<UPMesh> &navMesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");
	OPTICK_EVENT();

	uint32 cnt = numeric_cast<uint32>(navMesh->positions.size());
	std::vector<uint8> terrainTypes;
	terrainTypes.reserve(cnt);

	uint32 biomesCounts[256];
	uint32 typesCounts[256];
	for (uint32 i = 0; i < 256; i++)
	{
		biomesCounts[i] = 0;
		typesCounts[i] = 0;
	}

	for (uint32 i = 0; i < cnt; i++)
	{
		BiomeEnum b;
		TerrainTypeEnum t;
		functionTileProperties(navMesh->positions[i], navMesh->normals[i], b, t);
		terrainTypes.push_back((uint8)t);
		biomesCounts[(uint8)b]++;
		typesCounts[(uint8)t]++;
	}

	CAGE_LOG(SeverityEnum::Info, "biomes", "biomes:");
	for (uint32 i = 0; i < 256; i++)
	{
		if (biomesCounts[i])
		{
			BiomeEnum b = (BiomeEnum)i;
			CAGE_LOG_CONTINUE(SeverityEnum::Info, "biomes", statistics(stringizer() + b, biomesCounts[i], cnt));
		}
	}

	CAGE_LOG(SeverityEnum::Info, "terrainTypes", "terrain types:");
	for (uint32 i = 0; i < 256; i++)
	{
		if (typesCounts[i])
		{
			TerrainTypeEnum t = (TerrainTypeEnum)i;
			CAGE_LOG_CONTINUE(SeverityEnum::Info, "terrainTypes", statistics(stringizer() + t, typesCounts[i], cnt));
		}
	}

	return terrainTypes;
}


