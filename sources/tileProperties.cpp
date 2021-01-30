#include <cage-core/logger.h>
#include <cage-core/string.h>
#include <cage-core/polyhedron.h>

#include "terrain.h"
#include "generator.h"

namespace
{
	void statistics(const string &name, uint32 current, uint32 maxc, uint32 total)
	{
		const string c = stringizer() + current;
		const string r = stringizer() + 100 * real(current) / total;
		const string g = maxc > 0 ? fill(string(), 30 * current / maxc, '#') : string();
		CAGE_LOG_CONTINUE(SeverityEnum::Info, "tileStats", stringizer() + fill(name, 28) + reverse(fill(reverse(c), 6)) + " ~ " + reverse(fill(reverse(r), 12)) + " % " + g);
	}

	struct PropertyCounters
	{
		uint32 counts[256] = {};
		uint32 total = 0;
		uint32 maxc = 0;
		real a = 0;
		real b = 1;
		uint8 minIndex = m;
		uint8 maxIndex = 0;

		PropertyCounters(real a, real b) : a(a), b(b)
		{}

		void insert(real value)
		{
			uint8 index = numeric_cast<uint8>(clamp(255 * (value - a) / (b - a) + 0.5, 0, 255));
			counts[index]++;
			total++;
			maxc = max(maxc, counts[index]);
			minIndex = min(minIndex, index);
			maxIndex = max(maxIndex, index);
		}

		void print() const
		{
			real meanValue = real::Nan();
			uint32 meanCnt = 0;
			real sum = 0;
			for (uint32 i = minIndex; i <= maxIndex; i++)
			{
				real v = interpolate(a, b, (i / 255.0));
				statistics(stringizer() + v, counts[i], maxc, total);
				sum += v * counts[i];
				if (meanCnt < total / 2)
					meanValue = v;
				meanCnt += counts[i];
			}
			CAGE_LOG(SeverityEnum::Info, "tileStats", stringizer() + "average: " + (sum / total) + ", mean: " + meanValue);
			CAGE_LOG(SeverityEnum::Info, "tileStats", "");
		}
	};

	bool logFilterSameThread(const detail::LoggerInfo &info)
	{
		return info.createThreadId == info.currentThreadId;
	}
}

void generateTileProperties(const Holder<Polyhedron> &navMesh, std::vector<Tile> &tiles, const string &statsLogPath)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");

	CAGE_ASSERT(tiles.empty());

	Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(statsLogPath, false); // the file must be destroyed after the logger
	Holder<Logger> logger = newLogger();
	logger->filter.bind<&logFilterSameThread>();
	logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(+loggerFile);

	const uint32 cnt = navMesh->verticesCount();
	tiles.reserve(cnt);

	PropertyCounters elevations(-50, 150);
	PropertyCounters temperatures(-200, 200);
	PropertyCounters precipitations(0, 2000);
	PropertyCounters biomesCounts(0, 255);
	PropertyCounters typesCounts(0, 255);

	for (uint32 i = 0; i < cnt; i++)
	{
		Tile tile;
		tile.position = navMesh->position(i);
		tile.normal = navMesh->normal(i);
		terrainTileNavigation(tile);
		tiles.push_back(tile);

		elevations.insert(tile.elevation);
		temperatures.insert(tile.temperature);
		precipitations.insert(tile.precipitation);
		biomesCounts.insert((uint8)tile.biome);
		typesCounts.insert((uint8)tile.type);
	}

	CAGE_LOG(SeverityEnum::Info, "tileStats", "elevations:");
	elevations.print();

	CAGE_LOG(SeverityEnum::Info, "tileStats", "temperatures:");
	temperatures.print();

	CAGE_LOG(SeverityEnum::Info, "tileStats", "precipitations:");
	precipitations.print();

	CAGE_LOG(SeverityEnum::Info, "tileStats", "biomes:");
	for (uint32 i = 0; i < (uint32)TerrainBiomeEnum::_Total; i++)
	{
		TerrainBiomeEnum b = (TerrainBiomeEnum)i;
		statistics(stringizer() + b, biomesCounts.counts[i], biomesCounts.maxc, biomesCounts.total);
	}
	CAGE_LOG(SeverityEnum::Info, "tileStats", "");

	CAGE_LOG(SeverityEnum::Info, "tileStats", "terrain types:");
	for (uint32 i = 0; i < (uint32)TerrainTypeEnum::_Total; i++)
	{
		TerrainTypeEnum t = (TerrainTypeEnum)i;
		statistics(stringizer() + t, typesCounts.counts[i], typesCounts.maxc, typesCounts.total);
	}
	CAGE_LOG(SeverityEnum::Info, "tileStats", "");
}
