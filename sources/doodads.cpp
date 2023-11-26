#include <algorithm>

#include "planets.h"

#include <cage-core/files.h>
#include <cage-core/ini.h>
#include <cage-core/logger.h>
#include <cage-core/string.h>

namespace unnatural
{
	namespace
	{
		void loadDoodad(const String &root, const String &path)
		{
			Holder<Ini> ini = newIni();
			ini->importFile(path);
			DoodadDefinition d;
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
			doodadsDefinitions.push_back(std::move(d));
		}

		void loadDoodads(const String &root, const String &path)
		{
			Holder<PointerRange<String>> list = pathListDirectory(path);
			for (const String &p : list)
			{
				if (pathIsDirectory(p))
					loadDoodads(root, p);
				else if (isPattern(p, "", "", ".doodad"))
					loadDoodad(root, p);
			}
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

		const DoodadDefinition *chooseDoodad(const Tile &tile)
		{
			struct Eligible
			{
				const DoodadDefinition *doodad = nullptr;
				Real prob;
			};

			std::vector<Eligible> eligible;
			eligible.reserve(doodadsDefinitions.size());

			for (const DoodadDefinition &d : doodadsDefinitions)
			{
				if (d.ocean != (tile.biome == TerrainBiomeEnum::Water))
					continue;
				if (d.slope != (tile.type == TerrainTypeEnum::Cliffs))
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

		void printStatistics()
		{
			uint32 total = 0;
			uint32 maxc = 0;
			for (const DoodadDefinition &d : doodadsDefinitions)
			{
				total += d.instances;
				maxc = max(maxc, d.instances);
			}

			if (total == 0)
			{
				CAGE_LOG(SeverityEnum::Warning, "doodadStats", Stringizer() + "placed no doodads");
				return;
			}

			Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(pathJoin(baseDirectory, "doodads-stats.log"), false); // the file must be destroyed after the logger
			Holder<Logger> logger = newLogger();
			logger->filter.bind<&logFilterSameThread>();
			logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(+loggerFile);

			for (const DoodadDefinition &d : doodadsDefinitions)
			{
				const String c = Stringizer() + d.instances;
				const String r = Stringizer() + 100 * Real(d.instances) / total;
				const String g = maxc > 0 ? fill(String(), 30 * d.instances / maxc, '#') : "";
				CAGE_LOG_CONTINUE(SeverityEnum::Info, "doodadStats", Stringizer() + fill(d.name, 28) + reverse(fill(reverse(c), 6)) + " ~ " + reverse(fill(reverse(r), 12)) + " % " + g);
			}

			CAGE_LOG(SeverityEnum::Info, "doodadStats", Stringizer() + "placed " + total + " doodads in total (covers " + (100.0f * total / tiles.size()) + " % tiles)");
		}
	}

	void generateDoodads()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating doodads");

		const String root = pathSearchTowardsRoot("doodads", PathTypeFlags::Directory);
		loadDoodads(root, root);
		CAGE_LOG(SeverityEnum::Info, "doodads", Stringizer() + "found " + doodadsDefinitions.size() + " doodad prototypes");

		Holder<File> f = writeFile(pathJoin(baseDirectory, "doodads.ini"));
		for (Tile &t : tiles)
		{
			const DoodadDefinition *doodad = chooseDoodad(t);
			if (!doodad)
				continue;
			assetPackages.push_back(doodad->package);
			f->writeLine("[]");
			f->writeLine(Stringizer() + "prototype = " + doodad->proto);
			f->writeLine(Stringizer() + "position = " + t.position);
			f->writeLine("");
			const_cast<DoodadDefinition *>(doodad)->instances++;
			t.doodad = doodad;
		}
		f->close();

		printStatistics();
	}
}
