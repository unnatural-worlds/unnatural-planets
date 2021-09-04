#include <cage-core/logger.h>
#include <cage-core/string.h>
#include <cage-core/mesh.h>

#include "terrain.h"
#include "generator.h"

namespace
{
	void statistics(const String &name, uint32 current, uint32 maxc, uint32 total)
	{
		const String c = Stringizer() + current;
		const String r = Stringizer() + 100 * Real(current) / total;
		const String g = maxc > 0 ? fill(String(), 30 * current / maxc, '#') : String();
		CAGE_LOG_CONTINUE(SeverityEnum::Info, "tileStats", Stringizer() + fill(name, 28) + reverse(fill(reverse(c), 6)) + " ~ " + reverse(fill(reverse(r), 12)) + " % " + g);
	}

	struct PropertyCounters
	{
		uint32 counts[256] = {};
		uint32 total = 0;
		uint32 maxc = 0;
		Real a = 0;
		Real b = 1;
		uint8 minIndex = m;
		uint8 maxIndex = 0;

		PropertyCounters(Real a, Real b) : a(a), b(b)
		{}

		void insert(Real value)
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
			Real meanValue = Real::Nan();
			uint32 meanCnt = 0;
			Real sum = 0;
			for (uint32 i = minIndex; i <= maxIndex; i++)
			{
				Real v = interpolate(a, b, (i / 255.0));
				statistics(Stringizer() + v, counts[i], maxc, total);
				sum += v * counts[i];
				if (meanCnt < total / 2)
					meanValue = v;
				meanCnt += counts[i];
			}
			CAGE_LOG(SeverityEnum::Info, "tileStats", Stringizer() + "average: " + (sum / total) + ", mean: " + meanValue);
			CAGE_LOG(SeverityEnum::Info, "tileStats", "");
		}
	};

	bool logFilterSameThread(const detail::LoggerInfo &info)
	{
		return info.createThreadId == info.currentThreadId;
	}
}

void generateTileProperties(const Holder<Mesh> &navMesh, std::vector<Tile> &tiles, const String &statsLogPath)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");

	CAGE_ASSERT(tiles.empty());

	Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(statsLogPath, false); // the file must be destroyed after the logger
	Holder<Logger> logger = newLogger();
	logger->filter.bind<&logFilterSameThread>();
	logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(+loggerFile);

	const uint32 cnt = navMesh->verticesCount();
	tiles.reserve(cnt);

	PropertyCounters elevations(-5000, 5000);
	PropertyCounters temperatures(-150, 150);
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
		statistics(Stringizer() + b, biomesCounts.counts[i], biomesCounts.maxc, biomesCounts.total);
	}
	CAGE_LOG(SeverityEnum::Info, "tileStats", "");

	CAGE_LOG(SeverityEnum::Info, "tileStats", "terrain types:");
	for (uint32 i = 0; i < (uint32)TerrainTypeEnum::_Total; i++)
	{
		TerrainTypeEnum t = (TerrainTypeEnum)i;
		statistics(Stringizer() + t, typesCounts.counts[i], typesCounts.maxc, typesCounts.total);
	}
	CAGE_LOG(SeverityEnum::Info, "tileStats", "");
}
