#include <cage-core/files.h>
#include <cage-core/ini.h>
#include <cage-core/enumerate.h>
#include <cage-core/logger.h>

#include "generator.h"

#include <algorithm>

namespace
{
	struct Doodad
	{
		string name;
		uint32 instances = 0;

		string package;
		string proto;

		// requirements
		vec2 temperature;
		vec2 precipitation;
		real nationality;
		real fertility;
		real probability;
		bool ocean = false;
		bool slope = false;
	};

	Doodad loadDoodad(const string &root, const string &path)
	{
		Holder<Ini> ini = newIni();
		ini->importFile(path);
		Doodad d;
		d.name = pathToRel(path, root);
		d.package = ini->getString("doodad", "package");
		d.proto = ini->getString("doodad", "prototype");
		d.temperature = vec2::parse(ini->getString("requirements", "temperature", "-50, 50"));
		d.precipitation = vec2::parse(ini->getString("requirements", "precipitation", "0, 500"));
		d.nationality = ini->getFloat("requirements", "nationality", 0);
		d.fertility = ini->getFloat("requirements", "fertility", 0);
		d.probability = ini->getFloat("requirements", "probability", 0.15f);
		d.ocean = ini->getBool("requirements", "ocean", false);
		d.slope = ini->getBool("requirements", "slope", false);
		ini->checkUnused();
		if (!(d.temperature[0] < d.temperature[1]))
			CAGE_THROW_ERROR(Exception, "invalid temperature range");
		if (!(d.precipitation[0] < d.precipitation[1]))
			CAGE_THROW_ERROR(Exception, "invalid precipitation range");
		return d;
	}

	std::vector<Doodad> loadDoodads(const string &root, const string &path)
	{
		Holder<DirectoryList> dl = newDirectoryList(path);
		std::vector<Doodad> result;
		while (dl->valid())
		{
			if (dl->isDirectory())
			{
				auto ds = loadDoodads(root, pathJoin(path, dl->name()));
				result.insert(result.end(), ds.begin(), ds.end());
			}
			else if (dl->name().isPattern("", "", ".doodad"))
			{
				result.push_back(loadDoodad(root, pathJoin(path, dl->name())));
			}
			dl->next();
		}
		return result;
	}

	real factorInRange(const vec2 &range, real value)
	{
		const real m = (range[0] + range[1]) * 0.5;
		const real d = range[1] - m;
		const real v = 1 - (abs(value - m) / d);
		return max(0, v);
	}

	const Doodad *chooseDoodad(const std::vector<Doodad> &doodads, const vec3 &position, TerrainTypeEnum tileType, BiomeEnum biome, real elevation, real temperature, real precipitation)
	{
		real nationality, fertility;
		functionAuxiliaryProperties(position, nationality, fertility);

		struct Eligible
		{
			const Doodad *doodad;
			real prob;
		};

		std::vector<Eligible> eligible;
		eligible.reserve(doodads.size());

		for (const Doodad &d : doodads)
		{
			if (d.ocean != (biome == BiomeEnum::_Ocean))
				continue;
			if (d.slope != (tileType == TerrainTypeEnum::SteepSlope))
				continue;
			const real t = factorInRange(d.temperature, temperature);
			const real p = factorInRange(d.precipitation, precipitation);
			const real n = factorInRange(d.nationality + vec2(-1, 1), nationality);
			const real f = clamp(0.5 + fertility - d.fertility, 0, 1) * 2;
			Eligible e;
			e.prob = d.probability * t * p * n * f;
			CAGE_ASSERT(e.prob >= 0 && e.prob < 1);
			if (e.prob < 1e-3)
				continue;
			e.doodad = &d;
			eligible.push_back(e);
		}

		if (eligible.empty())
			return nullptr;

		std::sort(eligible.begin(), eligible.end(), [](const Eligible &a, const Eligible &b) {
			if (abs(a.prob - b.prob) < 1e-5)
				return a.doodad->instances < b.doodad->instances;
			return a.prob > b.prob;
			});

		real probSum = 0;
		for (const Eligible &e : eligible)
			probSum += e.prob;
		const real probMult = eligible[0].prob / probSum;
		for (Eligible &e : eligible)
			e.prob *= probMult;

		for (const Eligible &e : eligible)
			if (randomChance() < e.prob)
				return e.doodad;

		return nullptr;
	}

	void printStatistics(const std::vector<Doodad> &doodads, const uint32 verticesCount, const string &statsLogPath)
	{
		Holder<Logger> logger = newLogger();
		Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(statsLogPath, false);
		logger->format.bind<logFormatConsole>();
		logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(loggerFile.get());

		uint32 total = 0;
		uint32 maxc = 0;
		for (const Doodad &d : doodads)
		{
			total += d.instances;
			maxc = max(maxc, d.instances);
		}
		for (const Doodad &d : doodads)
		{
			const string c = stringizer() + d.instances;
			const string r = stringizer() + 100 * real(d.instances) / total;
			const string g = string().fill(30 * d.instances / maxc, '#');
			CAGE_LOG_CONTINUE(SeverityEnum::Info, "doodadStats", stringizer() + d.name.fill(28) + c.reverse().fill(6).reverse() + " ~ " + r.reverse().fill(9).reverse() + " % " + g);
		}
		CAGE_LOG(SeverityEnum::Info, "doodadStats", stringizer() + "placed " + total + " doodads in total (covers " + (100.0 * total / verticesCount) + " % tiles)");
	}
}

void generateDoodads(const Holder<Polyhedron> &navMesh, const std::vector<TerrainTypeEnum> &tileTypes, const std::vector<BiomeEnum> &tileBiomes, const std::vector<real> &tileElevations, const std::vector<real> &tileTemperatures, const std::vector<real> &tilePrecipitations, std::vector<string> &assetPackages, const string &doodadsPath, const string &statsLogPath)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating doodads");
	OPTICK_EVENT();

	CAGE_ASSERT(navMesh->verticesCount() == tileTypes.size());

	const string root = pathSearchTowardsRoot("doodads", PathTypeFlags::Directory);
	const std::vector<Doodad> doodads = loadDoodads(root, root);
	CAGE_LOG(SeverityEnum::Info, "doodads", stringizer() + "found " + doodads.size() + " doodad prototypes");

	Holder<File> f = writeFile(doodadsPath);
	for (const auto &it : enumerate(navMesh->positions()))
	{
		const uint32 i = numeric_cast<uint32>(it.cnt);
		const Doodad *doodad = chooseDoodad(doodads, *it, tileTypes[i], tileBiomes[i], tileElevations[i], tileTemperatures[i], tilePrecipitations[i]);
		if (!doodad)
			continue;
		assetPackages.push_back(doodad->package);
		f->writeLine("[]");
		f->writeLine(stringizer() + "prototype = " + doodad->proto);
		f->writeLine(stringizer() + "position = " + *it);
		f->writeLine("");
		const_cast<Doodad *>(doodad)->instances++;
	}
	f->close();

	printStatistics(doodads, navMesh->verticesCount(), statsLogPath);

	std::sort(assetPackages.begin(), assetPackages.end());
	assetPackages.erase(std::unique(assetPackages.begin(), assetPackages.end()), assetPackages.end());
}


