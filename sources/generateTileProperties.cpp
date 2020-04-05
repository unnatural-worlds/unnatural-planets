#include "generator.h"
#include "functions.h"

namespace
{
	void statistics(const string &name, uint32 current, uint32 maxc, uint32 total)
	{
		string c = stringizer() + current;
		string r = stringizer() + 100 * real(current) / total;
		string g = string().fill(30 * current / maxc, '#');
		CAGE_LOG_CONTINUE(SeverityEnum::Info, "tileStats", stringizer() + name.fill(28) + c.reverse().fill(6).reverse() + " ~ " + r.reverse().fill(9).reverse() + " % " + g);
	}

	struct propertyCounters
	{
		uint32 counts[256];
		uint32 total = 0;
		uint32 maxc = 0;
		real a = 0;
		real b = 1;
		uint8 minIndex = m;
		uint8 maxIndex = 0;

		propertyCounters(real a, real b) : a(a), b(b)
		{
			for (uint32 i = 0; i < 256; i++)
				counts[i] = 0;
		}

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

std::vector<uint8> generateTileProperties(const Holder<UPMesh> &navMesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");
	OPTICK_EVENT();

	uint32 cnt = numeric_cast<uint32>(navMesh->positions.size());
	std::vector<uint8> terrainTypes;
	terrainTypes.reserve(cnt);

	propertyCounters elevations(-50, 150);
	propertyCounters temperatures(-200, 200);
	propertyCounters precipitations(0, 2000);
	propertyCounters biomesCounts(0, 255);
	propertyCounters typesCounts(0, 255);

	for (uint32 i = 0; i < cnt; i++)
	{
		real e, t, p;
		BiomeEnum b;
		TerrainTypeEnum tt;
		functionTileProperties(navMesh->positions[i], navMesh->normals[i], b, tt, e, t, p);
		terrainTypes.push_back((uint8)tt);
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

	return terrainTypes;
}


