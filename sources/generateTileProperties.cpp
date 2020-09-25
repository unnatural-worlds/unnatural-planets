#include <cage-core/logger.h>
#include <cage-core/string.h>

#include "generator.h"

namespace
{
	void statistics(const string &name, uint32 current, uint32 maxc, uint32 total)
	{
		const string c = stringizer() + current;
		const string r = stringizer() + 100 * real(current) / total;
		const string g = fill(string(), 30 * current / maxc, '#');
		CAGE_LOG_CONTINUE(SeverityEnum::Info, "tileStats", stringizer() + fill(name, 28) + reverse(fill(reverse(c), 6)) + " ~ " + reverse(fill(reverse(r), 11)) + " % " + g);
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
			uint8 index = numeric_cast<uint8>(255 * (value - a) / (b - a) + 0.5);
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
}

void generateTileProperties(const Holder<Polyhedron> &navMesh, std::vector<TerrainTypeEnum> &tileTypes, std::vector<BiomeEnum> &tileBiomes, std::vector<real> &tileElevations, std::vector<real> &tileTemperatures, std::vector<real> &tilePrecipitations, const string &statsLogPath)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");
	OPTICK_EVENT();

	Holder<Logger> logger = newLogger();
	Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(statsLogPath, false);
	logger->format.bind<logFormatConsole>();
	logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(loggerFile.get());

	const uint32 cnt = navMesh->verticesCount();
	tileTypes.reserve(cnt);
	tileBiomes.reserve(cnt);
	tileElevations.reserve(cnt);
	tileTemperatures.reserve(cnt);
	tilePrecipitations.reserve(cnt);

	PropertyCounters elevations(-50, 150);
	PropertyCounters temperatures(-200, 200);
	PropertyCounters precipitations(0, 2000);
	PropertyCounters biomesCounts(0, 255);
	PropertyCounters typesCounts(0, 255);

	for (uint32 i = 0; i < cnt; i++)
	{
		real e, t, p;
		BiomeEnum b;
		TerrainTypeEnum tt;
		functionTileProperties(navMesh->position(i), navMesh->normal(i), b, tt, e, t, p);
		tileTypes.push_back(tt);
		tileBiomes.push_back(b);
		tileElevations.push_back(e);
		tileTemperatures.push_back(t);
		tilePrecipitations.push_back(p);
		elevations.insert(e);
		temperatures.insert(t);
		precipitations.insert(p);
		biomesCounts.insert((uint8)b);
		typesCounts.insert((uint8)tt);
	}

	CAGE_LOG(SeverityEnum::Info, "tileStats", "elevations:");
	elevations.print();

	CAGE_LOG(SeverityEnum::Info, "tileStats", "temperatures:");
	temperatures.print();

	CAGE_LOG(SeverityEnum::Info, "tileStats", "precipitations:");
	precipitations.print();

	CAGE_LOG(SeverityEnum::Info, "tileStats", "biomes:");
	for (uint32 i = 0; i < (uint32)BiomeEnum::_Total; i++)
	{
		BiomeEnum b = (BiomeEnum)i;
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


