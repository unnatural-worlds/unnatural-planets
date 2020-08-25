#include <cage-core/files.h>
#include <cage-core/ini.h>

#include "generator.h"

#include <algorithm>

namespace
{
	struct Doodad
	{
		string package;
		string proto;
	};

	Doodad loadDoodad(const string &path)
	{
		Holder<Ini> ini = newIni();
		ini->importFile(path);
		Doodad d;
		d.package = ini->getString("doodad", "package");
		d.proto = ini->getString("doodad", "prototype");
		ini->checkUnused();
		return d;
	}

	std::vector<Doodad> loadDoodads(const string &path)
	{
		Holder<DirectoryList> dl = newDirectoryList(path);
		std::vector<Doodad> result;
		while (dl->valid())
		{
			if (dl->isDirectory())
			{
				auto ds = loadDoodads(pathJoin(path, dl->name()));
				result.insert(result.end(), ds.begin(), ds.end());
			}
			else if (dl->name().isPattern("", "", ".doodad"))
			{
				result.push_back(loadDoodad(pathJoin(path, dl->name())));
			}
			dl->next();
		}
		return result;
	}
}

void generateDoodads(const Holder<Polyhedron> &navMesh, const std::vector<TerrainTypeEnum> &tileTypes, const std::vector<BiomeEnum> &tileBiomes, const std::vector<real> &tileElevations, const std::vector<real> &tileTemperatures, const std::vector<real> &tilePrecipitations, std::vector<string> &assetPackages, const string &doodadsPath)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating doodads");
	OPTICK_EVENT();

	CAGE_ASSERT(navMesh->verticesCount() == tileTypes.size());

	const std::vector<Doodad> doodads = loadDoodads(pathSearchTowardsRoot("doodads", PathTypeFlags::Directory));
	CAGE_LOG(SeverityEnum::Info, "doodads", stringizer() + "found " + doodads.size() + " doodad prototypes");

	Holder<File> f = writeFile(doodadsPath);
	uint32 total = 0;
	for (const vec3 &pos : navMesh->positions())
	{
		if (randomChance() > 0.1)
			continue;
		const uint32 idx = randomRange((uint32)0u, numeric_cast<uint32>(doodads.size()));
		assetPackages.push_back(doodads[idx].package);
		f->writeLine("[]");
		f->writeLine(stringizer() + "prototype = " + doodads[idx].proto);
		f->writeLine(stringizer() + "position = " + pos);
		f->writeLine("");
		total++;
	}
	f->close();
	CAGE_LOG(SeverityEnum::Info, "doodads", stringizer() + "placed " + total + "doodads");

	std::sort(assetPackages.begin(), assetPackages.end());
	assetPackages.erase(std::unique(assetPackages.begin(), assetPackages.end()), assetPackages.end());
}


