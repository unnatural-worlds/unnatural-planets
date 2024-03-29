#include <queue>

#include "planets.h"

#include <cage-core/config.h>
#include <cage-core/files.h>
#include <cage-core/flatSet.h>
#include <cage-core/geometry.h>
#include <cage-core/logger.h>
#include <cage-core/mesh.h>
#include <cage-core/spatialStructure.h>
#include <cage-core/string.h>

namespace unnatural
{
	void terrainTile(Tile &tile);

	namespace
	{
		void statistics(const String &name, uint32 current, uint32 maxc, uint32 total)
		{
			const String c = Stringizer() + current;
			const String r = Stringizer() + 100 * Real(current) / total;
			const String g = maxc > 0 ? fill(String(), 30 * current / maxc, '#') : String();
			CAGE_LOG_CONTINUE(SeverityEnum::Info, "tileStats", Stringizer() + fill(name, 28) + reverse(fill(reverse(c), 6)) + " ~ " + reverse(fill(reverse(r), 13)) + " % " + g);
		}

		template<uint32 Bins = 30>
		struct PropertyCounters
		{
			uint32 counts[Bins] = {};
			uint32 total = 0;
			uint32 maxc = 0;
			const Real a = 0;
			const Real b = 1;
			uint32 minIndex = m;
			uint32 maxIndex = 0;

			constexpr PropertyCounters(Real a, Real b) : a(a), b(b) {}

			constexpr void insert(uint32 index)
			{
				CAGE_ASSERT(index < Bins);
				counts[index]++;
				total++;
				maxc = max(maxc, counts[index]);
				minIndex = min(minIndex, index);
				maxIndex = max(maxIndex, index);
			}

			constexpr void insert(Real value)
			{
				const Real idx = Bins * (value - a) / (b - a);
				const uint32 index = min(numeric_cast<uint32>(max(idx, 0)), Bins - 1);
				insert(index);
			}

			void print() const
			{
				Real meanValue = Real::Nan();
				uint32 meanCnt = 0;
				Real sum = 0;
				for (uint32 i = minIndex; i <= maxIndex; i++)
				{
					const Real v = interpolate(a, b, (i / Real(Bins - 1)));
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

		constexpr bool testPropertyCounters()
		{
			PropertyCounters<12> cnt(0, 12);
			cnt.insert(Real(0));
			cnt.insert(Real(11));
			return cnt.counts[0] == 1 && cnt.counts[11] == 1;
		}

		static_assert(testPropertyCounters());

		bool logFilterSameThread(const detail::LoggerInfo &info)
		{
			return info.createThreadId == info.currentThreadId;
		}

		void computeNeighbors(const Holder<Mesh> &navMesh)
		{
			CAGE_ASSERT(navMesh->indicesCount());
			const uint32 cnt = navMesh->verticesCount();
			std::vector<FlatSet<uint32>> ns;
			ns.resize(cnt);
			switch (navMesh->type())
			{
				case MeshTypeEnum::Triangles:
				{
					const uint32 tris = navMesh->facesCount();
					const auto inds = navMesh->indices();
					for (uint32 t = 0; t < tris; t++)
					{
						const uint32 a = inds[t * 3 + 0];
						const uint32 b = inds[t * 3 + 1];
						const uint32 c = inds[t * 3 + 2];
						ns[a].insert(b);
						ns[a].insert(c);
						ns[b].insert(a);
						ns[b].insert(c);
						ns[c].insert(a);
						ns[c].insert(b);
					}
					break;
				}
				case MeshTypeEnum::Lines:
				{
					const uint32 lines = navMesh->facesCount();
					const auto inds = navMesh->indices();
					for (uint32 t = 0; t < lines; t++)
					{
						const uint32 a = inds[t * 2 + 0];
						const uint32 b = inds[t * 2 + 1];
						ns[a].insert(b);
						ns[b].insert(a);
					}
					break;
				}
				default:
					CAGE_THROW_CRITICAL(Exception, "invalid navmesh type");
			}
			for (uint32 i = 0; i < cnt; i++)
				tiles[i].neighbors = std::move(ns[i].unsafeData());
		}

		void computeFlatAreas()
		{
			const uint32 cnt = numeric_cast<uint32>(tiles.size());
			for (uint32 i = 0; i < cnt; i++)
			{
				struct Node
				{
					Real dist;
					uint32 id = m;

					bool operator<(const Node &other) const { return dist > other.dist; }
				};
				std::priority_queue<Node> open;
				FlatSet<uint32> closed;
				open.push({ Real(), i });
				const Vec3 p = tiles[i].position;
				const Vec3 n = tiles[i].normal;
				while (!open.empty())
				{
					const uint32 j = open.top().id;
					open.pop();
					if (closed.count(j))
						continue;
					closed.insert(j);
					const Vec3 v = tiles[j].position;
					if (abs(dot(v - p, n)) < 1.5)
					{
						for (uint32 k : tiles[j].neighbors)
							open.push({ distanceSquared(p, tiles[k].position), k });
					}
					else
					{
						tiles[i].flatRadius = distance(p, v);
						break;
					}
				}
			}

			{
				PropertyCounters flatsCounts(0, 200);
				for (const Tile &t : tiles)
					flatsCounts.insert(t.flatRadius);
				CAGE_LOG(SeverityEnum::Info, "tileStats", "flat areas radiuses (m):");
				flatsCounts.print();
			}
		}

		void computeBuildable()
		{
			const uint32 cnt = numeric_cast<uint32>(tiles.size());
			uint32 totalBuildable = 0;
			uint64 totalOverlapped = 0;
			Holder<SpatialStructure> spatStruct = newSpatialStructure({});
			for (uint32 i = 0; i < cnt; i++)
				spatStruct->update(i, tiles[i].position);
			spatStruct->rebuild();
			Holder<SpatialQuery> spatQuery = newSpatialQuery(spatStruct.share());
			for (uint32 i = 0; i < cnt; i++)
			{
				static constexpr Real Radius = 35;
				if (tiles[i].flatRadius < Radius)
					continue;
				if (tiles[i].type >= TerrainTypeEnum::Rough)
					continue;
				tiles[i].buildable = true;
				spatQuery->intersection(Sphere(tiles[i].position, Radius));
				for (uint32 j : spatQuery->result())
				{
					if (tiles[j].type >= TerrainTypeEnum::Rough)
					{
						tiles[i].buildable = false;
						break;
					}
				}
				if (tiles[i].buildable)
				{
					totalBuildable++;
					totalOverlapped += spatQuery->result().size();
				}
			}
			const uint32 totalBuildings = totalOverlapped ? numeric_cast<uint32>(uint64(totalBuildable) * totalBuildable / totalOverlapped) : 0;
			CAGE_LOG(SeverityEnum::Info, "tileStats", Stringizer() + "total tiles: " + tiles.size());
			CAGE_LOG(SeverityEnum::Info, "tileStats", Stringizer() + "buildable tiles: " + totalBuildable);
			CAGE_LOG(SeverityEnum::Info, "tileStats", Stringizer() + "total buildings: " + totalBuildings);
			CAGE_LOG(SeverityEnum::Info, "tileStats", "");

#if 0
			{
				const ConfigString configShapeMode("unnatural-planets/shape/mode");
				FileMode fm(false, true);
				fm.append = true;
				Holder<File> f = newFile("tiles-stats.csv", fm);
				f->writeLine(Stringizer() + String(configShapeMode) + "," + tiles.size() + "," + totalBuildings);
				f->close();
			}
#endif
		}
	}

	void generateTileProperties(const Holder<Mesh> &navMesh)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating tile properties");

		CAGE_ASSERT(tiles.empty());

		Holder<LoggerOutputFile> loggerFile = newLoggerOutputFile(pathJoin(baseDirectory, "tiles-stats.log"), false); // the file must be destroyed after the logger
		Holder<Logger> logger = newLogger();
		logger->filter.bind<&logFilterSameThread>();
		logger->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(+loggerFile);

		const uint32 cnt = navMesh->verticesCount();
		tiles.reserve(cnt);

		PropertyCounters elevations(-200, 600);
		PropertyCounters<8> slopes(10, 45);
		PropertyCounters temperatures(-50, 100);
		PropertyCounters precipitations(0, 500);
		PropertyCounters<(uint32)TerrainBiomeEnum::_Total> biomesCounts(0, (uint32)TerrainBiomeEnum::_Total);
		PropertyCounters<(uint32)TerrainTypeEnum::_Total> typesCounts(0, (uint32)TerrainTypeEnum::_Total);

		for (uint32 i = 0; i < cnt; i++)
		{
			Tile tile;
			tile.position = navMesh->position(i);
			tile.normal = navMesh->normal(i);
			tile.meshPurpose = MeshPurposeEnum::Navigation;
			terrainTile(tile);
			tiles.push_back(tile);

			elevations.insert(tile.elevation);
			slopes.insert(Degs(tile.slope).value);
			temperatures.insert(tile.temperature);
			precipitations.insert(tile.precipitation);
			biomesCounts.insert((uint32)tile.biome);
			typesCounts.insert((uint32)tile.type);
		}

		CAGE_LOG(SeverityEnum::Info, "tileStats", "elevations (m):");
		elevations.print();
		CAGE_LOG(SeverityEnum::Info, "tileStats", "slopes (°):");
		slopes.print();
		CAGE_LOG(SeverityEnum::Info, "tileStats", "temperatures (°C):");
		temperatures.print();
		CAGE_LOG(SeverityEnum::Info, "tileStats", "precipitations (mm):");
		precipitations.print();
		CAGE_LOG(SeverityEnum::Info, "tileStats", "biomes:");
		for (uint32 i = 0; i < (uint32)TerrainBiomeEnum::_Total; i++)
		{
			const TerrainBiomeEnum b = (TerrainBiomeEnum)i;
			statistics(Stringizer() + b, biomesCounts.counts[i], biomesCounts.maxc, biomesCounts.total);
		}
		CAGE_LOG(SeverityEnum::Info, "tileStats", "");
		CAGE_LOG(SeverityEnum::Info, "tileStats", "terrain types:");
		for (uint32 i = 0; i < (uint32)TerrainTypeEnum::_Total; i++)
		{
			const TerrainTypeEnum t = (TerrainTypeEnum)i;
			statistics(Stringizer() + t, typesCounts.counts[i], typesCounts.maxc, typesCounts.total);
		}
		CAGE_LOG(SeverityEnum::Info, "tileStats", "");

		computeNeighbors(navMesh);
		computeFlatAreas();
		computeBuildable();
	}
}
