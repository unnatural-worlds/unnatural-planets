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
						if (d < 300)
							return -Real::Infinity(); // starting positions too close to each other
						dists.push_back(d);
					}
				}
			}
			return meanMinusDeviation(dists);
		}

		struct Candidate
		{
			uint32 pos = m;
			Real maxResourceDist = 0;
		};

		std::vector<Candidate> makeCandidates(const std::vector<Tile> &tiles)
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
			for (auto &it : dds)
			{
				if (it.second.ps.size() < it.first->starting)
				{
					CAGE_LOG_THROW(Stringizer() + "doodad: " + it.first->name);
					CAGE_THROW_ERROR(Exception, "insufficient number of doodads to generate starting positions");
				}
			}

			std::vector<Candidate> candidates;
			for (const auto &it : dds)
			{
				for (uint32 p : it.second.ps)
				{
					Candidate c;
					c.pos = p;
					for (auto &d : dds)
					{
						CAGE_ASSERT(d.second.ps.size() >= d.first->starting);
						d.second.ds.clear();
						for (const auto &jt : d.second.ps)
							d.second.ds.push_back(distance(tiles[jt].position, tiles[p].position));
						std::sort(d.second.ds.begin(), d.second.ds.end());
						c.maxResourceDist = max(c.maxResourceDist, d.second.ds[d.first->starting]);
					}
					candidates.push_back(c);
				}
			}

			std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) { return a.maxResourceDist < b.maxResourceDist; });
			if (candidates.size() > 100)
				candidates.resize(100);

			return candidates;
		}

		Real evaluateCandidates(const std::vector<Tile> &tiles, PointerRange<const Candidate> candidates)
		{
			std::vector<uint32> ps;
			for (const Candidate &c : candidates)
				ps.push_back(c.pos);
			return scoreEquidistantPositions(tiles, ps);
		}
	}

	void generateStartingPositions(const Holder<Mesh> &navMesh, const std::vector<Tile> &tiles, const String &startsPath)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating starting positions");

		CAGE_ASSERT(navMesh->verticesCount() == tiles.size());

		const std::vector<Candidate> allCandidates = makeCandidates(tiles);

		PointerRange<const Candidate> bestSolution;
		Real bestScore = -Real::Infinity();
		for (uint32 pc = 5; pc < 11; pc++)
		{
			if (allCandidates.size() < pc)
				break;
			for (uint32 off = 0; off < allCandidates.size() - pc; off++)
			{
				PointerRange<const Candidate> pr = { allCandidates.data() + off, allCandidates.data() + off + pc };
				const Real score = evaluateCandidates(tiles, pr);
				if (score > bestScore)
				{
					bestSolution = pr;
					bestScore = score;
				}
			}
		}
		if (bestSolution.empty())
			CAGE_THROW_ERROR(Exception, "generated no starting positions");

		{
			Holder<File> f = writeFile(startsPath);
			f->writeLine("[]");
			for (const Candidate &c : bestSolution)
				f->writeLine(Stringizer() + tiles[c.pos].position);
			f->close();
		}

		{
			Holder<Mesh> msh = newMesh();
			msh->type(MeshTypeEnum::Lines);
			for (const Candidate &c : bestSolution)
			{
				const Vec3 a = tiles[c.pos].position;
				msh->addLine(makeSegment(a, a + tiles[c.pos].normal * 100));
			}
			msh->exportFile(startsPath + "-preview.obj");
		}

		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "generated " + bestSolution.size() + " starting positions");
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "starting positions score: " + bestScore);
	}
}
