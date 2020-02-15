#include "generator.h"
#include "functions.h"

std::vector<uint8> generateTileProperties(const Holder<UPMesh> &navMesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");
	OPTICK_EVENT();

	uint32 cnt = numeric_cast<uint32>(navMesh->positions.size());
	std::vector<uint8> terrainTypes;
	terrainTypes.reserve(cnt);
	for (uint32 i = 0; i < cnt; i++)
	{
		uint8 type;
		functionTileProperties(navMesh->positions[i], navMesh->normals[i], type);
		terrainTypes.push_back(type);
	}
	printTerrainTypeStatistics(terrainTypes);
	return terrainTypes;
}


