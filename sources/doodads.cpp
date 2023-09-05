#include <algorithm>

#include "doodads.h"
#include "generator.h"
#include "tile.h"

#include <cage-core/enumerate.h>
#include <cage-core/files.h>
#include <cage-core/ini.h>
#include <cage-core/logger.h>
#include <cage-core/mesh.h>
#include <cage-core/pointerRangeHolder.h>
#include <cage-core/string.h>

namespace unnatural
{
	namespace
	{
		Doodad loadDoodad(const String &root, const String &path)
		{
			Holder<Ini> ini = newIni();
			ini->importFile(path);
			Doodad d;
			d.name = pathToRel(path, root);
			d.package = ini->getString("doodad", "package");
			d.proto = ini->getString("doodad", "prototype");
			if (ini->itemExists("requirements", "temperature"))
				d.temperature = Vec2::parse(ini->getString("requirements", "temperature"));
			if (ini->itemExists("requirements", "precipitation"))
				d.precipitation = Vec2::parse(ini->getString("requirements", "precipitation"));
			d.probability = ini->getFloat("requirements", "probability", 0.05f);
			d.starting = ini->getUint32("requirements", "starting", 0);
			d.ocean = ini->getBool("requirements", "ocean", false);
			d.slope = ini->getBool("requirements", "slope", false);
			d.buildable = ini->getBool("requirements", "buildable", false);
			ini->checkUnused();
			if (!(d.temperature[0] < d.temperature[1]))
				CAGE_THROW_ERROR(Exception, "invalid temperature range");
			if (!(d.precipitation[0] < d.precipitation[1]))
				CAGE_THROW_ERROR(Exception, "invalid precipitation range");
			return d;
		}

		Holder<PointerRange<Doodad>> loadDoodads(const String &root, const String &path)
		{
			PointerRangeHolder<Doodad> result;
			Holder<PointerRange<String>> list = pathListDirectory(path);
			for (const String &p : list)
			{
				if (pathIsDirectory(p))
				{
					auto ds = loadDoodads(root, p);
					result.insert(result.end(), ds.begin(), ds.end());
				}
				else if (isPattern(p, "", "", ".doodad"))
					result.push_back(loadDoodad(root, p));
			}
			return result;
		}

		Real factorInRange(const Vec2 &range, Real value)
		{
			if (range[1] - range[0] == Real::Infinity())
				return 1;
			const Real m = (range[0] + range[1]) * 0.5;
			const Real d = range[1] - m;
			const Real v = 1 - (abs(value - m) / d);
			return max(0, v);
		}

		const Doodad *chooseDoodad(PointerRange<const Doodad> doodads, const Tile &tile)
		{
			struct Eligible
			{
				const Doodad *doodad = nullptr;
				Real prob;
			};

			std::vector<Eligible> eligible;
			eligible.reserve(doodads.size());

			for (const Doodad &d : doodads)
			{
				if (d.ocean != (tile.biome == TerrainBiomeEnum::Water))
					continue;
				if (d.slope != (tile.type == TerrainTypeEnum::SteepSlope))
					continue;
				if (d.buildable && !tile.buildable)
					continue;
				const Real t = factorInRange(d.temperature, tile.temperature);
				const Real p = factorInRange(d.precipitation, tile.precipitation);
				Eligible e;
				e.prob = d.probability * t * p;
				CAGE_ASSERT(e.prob >= 0 && e.prob < 1);
				if (e.prob < 1e-5)
					continue;
				e.doodad = &d;
				eligible.push_back(e);
			}

			if (eligible.empty())
				return nullptr;

			std::sort(eligible.begin(), eligible.end(),
				[](const Eligible &a, const Eligible &b)
				{
					if (abs(a.prob - b.prob) < 1e-5)
						return a.doodad->instances < b.doodad->instances;
					return a.prob > b.prob;
				});

			Real probSum = 0;
			for (const Eligible &e : eligible)
				probSum += e.prob;
			const Real probMult = eligible[0].prob / probSum;
			for (Eligible &e : eligible)
				e.prob *= probMult;

			for (const Eligible &e : eligible)
				if (randomChance() < e.prob)
					return e.doodad;

			return nullptr;
		}

		bool logFilterSameThread(const detail::LoggerInfo &info)
		{
			return info.createThreadId == info.currentThreadId;
		}

		void printStatistics(PointerRange<const Doodad> doodads, const uint32 verticesCount, const String &statsLogPath)
		{
			uint32 total = 0;
			uint32 maxc = 0;
			for (const Doodad &d : doodads)
			{
				total += d.instances;
				maxc = max(maxc, d.instances);
			}

			if (total == 0)
			{
				CAGE_LOG(SeverityEnum::Warning, "doodadStats", Stringizer() + "placed no doodads");
				return;
			}

			Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(statsLogPath, false); // the file must be destroyed after the logger
			Holder<Logger> logger = newLogger();
			logger->filter.bind<&logFilterSameThread>();
			logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(+loggerFile);

			for (const Doodad &d : doodads)
			{
				const String c = Stringizer() + d.instances;
				const String r = Stringizer() + 100 * Real(d.instances) / total;
				const String g = maxc > 0 ? fill(String(), 30 * d.instances / maxc, '#') : "";
				CAGE_LOG_CONTINUE(SeverityEnum::Info, "doodadStats", Stringizer() + fill(d.name, 28) + reverse(fill(reverse(c), 6)) + " ~ " + reverse(fill(reverse(r), 12)) + " % " + g);
			}

			CAGE_LOG(SeverityEnum::Info, "doodadStats", Stringizer() + "placed " + total + " doodads in total (covers " + (100.0f * total / verticesCount) + " % tiles)");
		}
	}

	Holder<PointerRange<Doodad>> generateDoodads(const Holder<Mesh> &navMesh, std::vector<Tile> &tiles, std::vector<String> &assetPackages, const String &doodadsPath, const String &statsLogPath)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating doodads");

		CAGE_ASSERT(navMesh->verticesCount() == tiles.size());

		const String root = pathSearchTowardsRoot("doodads", PathTypeFlags::Directory);
		Holder<PointerRange<Doodad>> doodads = loadDoodads(root, root);
		CAGE_LOG(SeverityEnum::Info, "doodads", Stringizer() + "found " + doodads.size() + " doodad prototypes");

		Holder<File> f = writeFile(doodadsPath);
		for (const auto &it : enumerate(navMesh->positions()))
		{
			const uint32 i = numeric_cast<uint32>(it.index);
			const Doodad *doodad = chooseDoodad(doodads, tiles[i]);
			if (!doodad)
				continue;
			assetPackages.push_back(doodad->package);
			f->writeLine("[]");
			f->writeLine(Stringizer() + "prototype = " + doodad->proto);
			f->writeLine(Stringizer() + "position = " + *it);
			f->writeLine("");
			const_cast<Doodad *>(doodad)->instances++;
			tiles[i].doodad = doodad;
		}
		f->close();

		printStatistics(doodads, navMesh->verticesCount(), statsLogPath);

		std::sort(assetPackages.begin(), assetPackages.end());
		assetPackages.erase(std::unique(assetPackages.begin(), assetPackages.end()), assetPackages.end());

		return doodads;
	}
}
