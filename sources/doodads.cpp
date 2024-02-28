#include <algorithm>
#include <tuple>

#include "planets.h"

#include <cage-core/enumerate.h>
#include <cage-core/files.h>
#include <cage-core/geometry.h>
#include <cage-core/ini.h>
#include <cage-core/logger.h>
#include <cage-core/math.h>
#include <cage-core/mesh.h>
#include <cage-core/spatialStructure.h>
#include <cage-core/string.h>

namespace unnatural
{
	void previewMeshAddPoint(Mesh *msh, Vec3 pos, Vec3 up, Real height);

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

			d.priority = ini->getFloat("generating", "priority", d.priority.value);
			d.maxCount = ini->getUint32("generating", "count", d.maxCount);
			d.chance = ini->getFloat("generating", "chance", d.chance.value);

			if (ini->itemExists("requirements", "temperature"))
				d.temperature = Vec2::parse(ini->getString("requirements", "temperature"));
			if (ini->itemExists("requirements", "precipitation"))
				d.precipitation = Vec2::parse(ini->getString("requirements", "precipitation"));
			if (ini->itemExists("requirements", "elevation"))
				d.elevation = Vec2::parse(ini->getString("requirements", "elevation"));
			if (ini->itemExists("requirements", "slope"))
			{
				d.slope = Vec2::parse(ini->getString("requirements", "slope"));
				d.slope[0] = Rads(Degs(d.slope[0])).value;
				d.slope[1] = Rads(Degs(d.slope[1])).value;
			}
			if (ini->itemExists("starting", "distance"))
				d.startsDistance = Vec2::parse(ini->getString("starting", "distance"));
			if (ini->itemExists("starting", "count"))
				d.startsCount = Vec2i::parse(ini->getString("starting", "count"));
			d.radius = ini->getFloat("requirements", "radius", d.radius.value);
			d.buildable = ini->getBool("requirements", "buildable", false);

			if (ini->itemExists("preview", "height"))
				d.previewHeight = ini->getFloat("preview", "height");

			ini->checkUnused();
			if (!(d.temperature[0] < d.temperature[1]))
				CAGE_THROW_ERROR(Exception, "invalid temperature range");
			if (!(d.precipitation[0] < d.precipitation[1]))
				CAGE_THROW_ERROR(Exception, "invalid precipitation range");
			if (!(d.elevation[0] < d.elevation[1]))
				CAGE_THROW_ERROR(Exception, "invalid elevation range");
			if (!(d.slope[0] < d.slope[1]))
				CAGE_THROW_ERROR(Exception, "invalid slope range");
			if (!(d.startsDistance[0] <= d.startsDistance[1]))
				CAGE_THROW_ERROR(Exception, "invalid starting distance range");
			if (!(d.startsCount[0] <= d.startsCount[1]))
				CAGE_THROW_ERROR(Exception, "invalid starting count range");
			if (!(d.radius > 0))
				CAGE_THROW_ERROR(Exception, "invalid radius");

			doodadsDefinitions.push_back(std::move(d));
		}

		void loadDoodads(const String &root, const String &path)
		{
			Holder<PointerRange<String>> list = pathListDirectory(path);
			for (const String &p : list)
			{
				try
				{
					if (pathIsDirectory(p))
						loadDoodads(root, p);
					else if (isPattern(p, "", "", ".doodad"))
						loadDoodad(root, p);
					else
						CAGE_THROW_ERROR(Exception, "path is not doodad");
				}
				catch (...)
				{
					CAGE_LOG_THROW(p);
					throw;
				}
			}
		}

		Real factorInRange(const Vec2 &range, Real value)
		{
			if (range[1] - range[0] == Real::Infinity())
				return 1;
			const Real m = (range[0] + range[1]) * 0.5;
			const Real d = range[1] - m;
			const Real v = 1 - (abs(value - m) / d);
			return saturate(v);
		}

		Holder<SpatialStructure> spatialStructure = newSpatialStructure({});
		Holder<SpatialQuery> spatialQuery = newSpatialQuery(spatialStructure.share());

		void placeDoodads(DoodadDefinition &doodad)
		{
			std::vector<uint32> spCounts; // count doodads for each starting position
			spCounts.resize(startingPositions.size(), 0);

			const auto &tryPlace = [&doodad, &spCounts](uint32 c) -> bool
			{
				if (doodad.startsDistance[0] > 0)
				{
					for (uint32 s : startingPositions)
						if (distance(tiles[s].position, tiles[c].position) < doodad.startsDistance[0])
							return false; // doodad too close to a starting position
				}

				if (doodad.startsDistance[1] > 0)
				{
					for (auto ss : enumerate(startingPositions))
					{
						if (distance(tiles[*ss].position, tiles[c].position) > doodad.startsDistance[1])
							continue;
						if (spCounts[ss.index] >= doodad.startsCount[1])
							return false; // too many doodads close to a starting position
					}
				}

				Tile &t = tiles[c];
				spatialQuery->intersection(Sphere(t.position, doodad.radius));
				if (!spatialQuery->result().empty())
					return false; // would overlap with already existing doodad

				t.buildable = false;
				t.doodad = &doodad;
				doodad.instances++;
				spatialStructure->update(c, Sphere(t.position, doodad.radius));
				spatialStructure->rebuild();

				if (doodad.startsDistance[1] > 0)
				{
					for (auto ss : enumerate(startingPositions))
					{
						if (distance(tiles[*ss].position, tiles[c].position) > doodad.startsDistance[1])
							continue;
						spCounts[ss.index]++;
					}
				}

				return true;
			};

			// generate initial candidates
			std::vector<uint32> candidates;
			candidates.reserve(tiles.size() / 10);
			for (auto it : enumerate(tiles))
			{
				if (it->doodad)
					continue;
				if (doodad.buildable && !it->buildable)
					continue;
				const Real t = factorInRange(doodad.temperature, it->temperature);
				const Real p = factorInRange(doodad.precipitation, it->precipitation);
				const Real e = factorInRange(doodad.elevation, it->elevation);
				const Real s = factorInRange(doodad.slope, it->slope.value);
				if (t * p * e * s < 0.001)
					continue;
				candidates.push_back(it.index);
			}

			// shuffle the candidates
			const auto &shuffle = [&candidates]()
			{
				for (uint32 &c : candidates)
					std::swap(c, candidates[randomRange(0u, numeric_cast<uint32>(candidates.size()))]);
			};
			shuffle();

			// place doodads
			for (uint32 c : candidates)
			{
				if (doodad.instances >= doodad.maxCount)
					break;
				if (randomChance() > doodad.chance)
					continue;
				tryPlace(c);
			}
			std::erase_if(candidates, [](uint32 c) { return !!tiles[c].doodad; });

			// place additional doodads to meet minimum requirements for starting positions
			for (uint32 player = 0; player < startingPositions.size(); player++)
			{
				uint32 &myCount = spCounts[player];
				if (myCount >= doodad.startsCount[0])
					continue;
				shuffle();
				const Vec3 myPos = tiles[startingPositions[player]].position;
				for (uint32 c : candidates)
				{
					const Real dist = distance(tiles[c].position, myPos);
					if (dist < doodad.startsDistance[0] || dist > doodad.startsDistance[1])
						continue;
					tryPlace(c);
					if (myCount >= doodad.startsCount[0])
						break;
				}
			}
		}

		void writeDoodads()
		{
			{
				Holder<File> f = writeFile(pathJoin(baseDirectory, "doodads.ini"));
				for (Tile &t : tiles)
				{
					if (!t.doodad)
						continue;
					assetPackages.push_back(t.doodad->package);
					f->writeLine("[]");
					f->writeLine(Stringizer() + "prototype = " + t.doodad->proto);
					f->writeLine(Stringizer() + "position = " + t.position);
					f->writeLine("");
				}
				f->close();
			}

			{
				Holder<Mesh> msh = newMesh();
				for (Tile &t : tiles)
				{
					if (!t.doodad)
						continue;
					if (!valid(t.doodad->previewHeight))
						continue;
					previewMeshAddPoint(+msh, t.position, t.normal, t.doodad->previewHeight);
				}
				msh->exportFile(pathJoin(baseDirectory, "doodads-preview.obj"));
			}
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
		CAGE_LOG(SeverityEnum::Info, "doodads", Stringizer() + "found " + doodadsDefinitions.size() + " doodads definitions");

		std::sort(doodadsDefinitions.begin(), doodadsDefinitions.end(),
			[](const DoodadDefinition &a, const DoodadDefinition &b)
			{
				const auto aa = std::tuple(-a.priority, a.maxCount, a.chance, a.startsDistance[0]);
				const auto bb = std::tuple(-b.priority, b.maxCount, b.chance, b.startsDistance[0]);
				return aa < bb;
			});
		for (DoodadDefinition &doodad : doodadsDefinitions)
			placeDoodads(doodad);

		writeDoodads();
		printStatistics();
	}
}
