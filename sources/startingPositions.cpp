#include <algorithm>
#include <map>
#include <random>

#include "doodads.h"
#include "generator.h"
#include "tile.h"

#include <cage-core/enumerate.h>
#include <cage-core/files.h>
#include <cage-core/geometry.h>
#include <cage-core/mesh.h>

namespace unnatural
{
	namespace
	{
		Real meanMinusDeviation(PointerRange<const Real> values)
		{
			Real avg = 0;
			for (Real s : values)
				avg += s;
			avg /= values.size();
			Real dev = 0;
			for (Real s : values)
				dev += abs(s - avg);
			dev /= values.size();
			return avg - dev;
		}

		Real scoreEquidistantPositions(const std::vector<Tile> &tiles, PointerRange<const uint32> positions)
		{
			std::vector<Real> dists;
			for (uint32 i : positions)
			{
				for (uint32 j : positions)
				{
					if (j > i)
					{
						const Real d = distance(tiles[i].position, tiles[j].position);
						if (d < 250)
							return -Real::Infinity(); // starting positions too close to each other
						dists.push_back(d);
					}
				}
			}
			return meanMinusDeviation(dists);
		}

		Real scoreStartingResources(const std::vector<Tile> &tiles, PointerRange<const uint32> positions)
		{
			struct Dd
			{
				std::vector<uint32> ps; // positions
				std::vector<Real> ds; // distances
			};
			std::map<const Doodad *, Dd> dds;
			for (auto it : enumerate(tiles))
				if (it->doodad && it->doodad->starting > 0)
					dds[it->doodad].ps.push_back(numeric_cast<uint32>(it.index));
			uint32 totalDoodads = 0;
			for (auto &it : dds)
			{
				if (it.second.ps.size() < it.first->starting)
				{
					CAGE_LOG_THROW(Stringizer() + "doodad: " + it.first->name);
					CAGE_THROW_ERROR(Exception, "insufficient number of doodads to generate starting positions");
				}
				totalDoodads += it.first->starting;
			}

			std::vector<Real> dists;
			for (uint32 start : positions)
			{
				Real score = 0;
				for (auto &it : dds)
				{
					it.second.ds.clear();
					for (uint32 d : it.second.ps)
						it.second.ds.push_back(distance(tiles[d].position, tiles[start].position));
					std::sort(it.second.ds.begin(), it.second.ds.end());
					it.second.ds.resize(it.first->starting);
					for (Real d : it.second.ds)
						score += 500 - max(d - 500, 0);
				}
				dists.push_back(score);
			}
			return meanMinusDeviation(dists);
		}

		Real calculateScore(const std::vector<Tile> &tiles, PointerRange<const uint32> positions)
		{
			const Real edp = scoreEquidistantPositions(tiles, positions);
			if (edp == -Real::Infinity())
				return edp;
			const Real sr = scoreStartingResources(tiles, positions);
			CAGE_LOG_DEBUG(SeverityEnum::Info, "generator", Stringizer() + "score edp: " + edp + ", sr: " + sr + ", count: " + positions.size());
			return edp + sr;
		}

		std::vector<uint32> pickRandomPositions(const std::vector<Tile> &tiles)
		{
			std::vector<uint32> candidates;
			for (uint32 i = 0; i < tiles.size(); i++)
				if (tiles[i].buildable)
					candidates.push_back(i);
			if (candidates.size() < 3)
				CAGE_THROW_ERROR(Exception, "not enough candidates for starting positions");
			std::shuffle(candidates.begin(), candidates.end(), std::mt19937(randomRange(uint32(0), uint32(m))));
			uint32 count = randomRange(6, 15);
			count = min(count, numeric_cast<uint32>(candidates.size()));
			candidates.resize(count);
			return candidates;
		}
	}

	void generateStartingPositions(const Holder<Mesh> &navMesh, const std::vector<Tile> &tiles, const String &startsPath)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating starting positions");

		CAGE_ASSERT(navMesh->verticesCount() == tiles.size());

		std::vector<uint32> bestSolution;
		Real bestScore = -Real::Infinity();

		for (uint32 attempt = 0; attempt < 1000; attempt++)
		{
			std::vector<uint32> current = pickRandomPositions(tiles);
			const Real score = calculateScore(tiles, current);
			if (score > bestScore)
			{
				std::swap(bestSolution, current);
				bestScore = score;
			}
		}

		if (bestSolution.empty())
			CAGE_THROW_ERROR(Exception, "generated no starting positions");

		{
			Holder<File> f = writeFile(startsPath);
			f->writeLine("[]");
			for (uint32 position : bestSolution)
				f->writeLine(Stringizer() + tiles[position].position);
			f->close();
		}

		{
			Holder<Mesh> msh = newMesh();
			msh->type(MeshTypeEnum::Lines);
			for (uint32 p : bestSolution)
			{
				const Vec3 a = tiles[p].position;
				msh->addLine(makeSegment(a, a + tiles[p].normal * 100));
			}
			msh->exportFile(startsPath + "-preview.obj");
		}

		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "generated " + bestSolution.size() + " starting positions");
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "starting positions score: " + bestScore);
	}
}
