#include "math.h" // anyPerpendicular
#include "planets.h"

#include <cage-core/enumerate.h>
#include <cage-core/files.h>
#include <cage-core/flatSet.h>
#include <cage-core/geometry.h>
#include <cage-core/mesh.h>
#include <cage-core/spatialStructure.h>

namespace unnatural
{
	void previewMeshAddPoint(Mesh *msh, Vec3 pos, Vec3 up, Real height);

	namespace
	{
		Real minimumDistance(PointerRange<const uint32> positions)
		{
			const uint32 n = positions.size();
			if (n < 2)
				return 0;
			Real score = Real::Infinity();
			for (uint32 i = 0; i < n - 1; i++)
			{
				for (uint32 j = i + 1; j < n; j++)
				{
					const Real c = distance(tiles[positions[i]].position, tiles[positions[j]].position);
					score = min(score, c);
				}
			}
			return score;
		}

		void filterPositionsByBuildableRadius(std::vector<uint32> &positions)
		{
			Holder<SpatialStructure> spatStruct = newSpatialStructure({});
			for (auto it : enumerate(tiles))
				spatStruct->update(it.index, it->position);
			spatStruct->rebuild();
			Holder<SpatialQuery> spatQuery = newSpatialQuery(spatStruct.share());

			std::erase_if(positions,
				[&](uint32 i)
				{
					spatQuery->intersection(Sphere(tiles[i].position, 300));
					uint32 b = 0;
					for (uint32 i : spatQuery->result())
						if (tiles[i].buildable)
							b++;
					static constexpr uint32 Threshold = CAGE_DEBUG_BOOL ? 200 : 2000;
					return b < Threshold;
				});
		}

		void filterPositionsByLargestConnectedWalkableComponent(std::vector<uint32> &positions)
		{
			const auto &walkable = [](uint32 i) -> bool
			{
				switch (tiles[i].type)
				{
					case TerrainTypeEnum::Road:
					case TerrainTypeEnum::Flat:
					case TerrainTypeEnum::Rough:
						return true;
					default:
						return false;
				}
			};

			const uint32 cnt = tiles.size();
			std::vector<uint32> component;
			component.resize(cnt, m);

			uint32 largestSize = 0;
			uint32 largestIndex = m;

			const auto &floodfill = [&](const uint32 start)
			{
				const uint32 index = component[start];
				uint32 size = 1;
				std::vector<uint32> open;
				open.reserve(cnt / 10);
				open.push_back(start);
				while (!open.empty())
				{
					const uint32 i = open.back();
					open.pop_back();
					CAGE_ASSERT(component[i] == index);
					for (uint32 j : tiles[i].neighbors)
					{
						if (component[j] != m || !walkable(j))
							continue;
						component[j] = index;
						size++;
						open.push_back(j);
					}
				}
				if (size > largestSize)
				{
					largestSize = size;
					largestIndex = index;
				}
			};

			uint32 next = 0;
			for (uint32 i = 0; i < cnt; i++)
			{
				if (component[i] != m || !walkable(i))
					continue;
				component[i] = next;
				floodfill(i);
				next++;
			}

			std::erase_if(positions, [&](uint32 i) { return component[i] != largestIndex; });
		}

		std::vector<uint32> generateCadidates()
		{
			std::vector<uint32> candidates;
			candidates.reserve(tiles.size() / 2);
			for (auto it : enumerate(tiles))
				if (it->buildable)
					candidates.push_back(it.index);
			filterPositionsByBuildableRadius(candidates);
			filterPositionsByLargestConnectedWalkableComponent(candidates);
			CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "starting position candidates: " + candidates.size());
			return candidates;
		}

		std::vector<uint32> proposeSolution(const std::vector<uint32> &candidates)
		{
			FlatSet<uint32> proposal;
			proposal.reserve(100);
			for (uint32 attempt = 0; attempt < 1000; attempt++)
			{
				const uint32 p = candidates[randomRange(std::size_t{}, candidates.size())];
				bool valid = true;
				for (uint32 a : proposal)
					if (distance(tiles[p].position, tiles[a].position) < 1000)
						valid = false; // starting positions too close to each other
				if (!valid)
					continue;
				proposal.insert(p);
				if (proposal.size() >= 20)
					break;
			}
			return std::move(proposal.unsafeData());
		}
	}

	void generateStartingPositions()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating starting positions");

		const std::vector<uint32> allCandidates = generateCadidates();
		if (allCandidates.empty())
			CAGE_THROW_ERROR(Exception, "no starting positions candidates");

		Real bestScore = 0;
		static constexpr uint32 Limit = 100000 / (CAGE_DEBUG_BOOL ? 100 : 1);
		for (uint32 attempt = 0; attempt < Limit; attempt++)
		{
			std::vector<uint32> proposal = proposeSolution(allCandidates);
			if (proposal.size() < 2)
				continue;
			if (proposal.size() < startingPositions.size())
				continue;
			const Real score = minimumDistance(proposal);
			if (proposal.size() > startingPositions.size() || score > bestScore)
			{
				startingPositions = std::move(proposal);
				bestScore = score;
			}
		}
		if (startingPositions.empty())
			CAGE_THROW_ERROR(Exception, "generated insufficient starting positions");

		{
			Holder<File> f = writeFile(pathJoin(baseDirectory, "starts.ini"));
			f->writeLine("[]");
			for (uint32 c : startingPositions)
				f->writeLine(Stringizer() + tiles[c].position);
			f->close();
		}

		{
			Holder<Mesh> msh = newMesh();
			for (uint32 i : startingPositions)
				previewMeshAddPoint(+msh, tiles[i].position, tiles[i].normal, 200);
			msh->exportFile(pathJoin(baseDirectory, "starts-preview.obj"));
		}

		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "generated " + startingPositions.size() + " starting positions");
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "starting positions score: " + bestScore);
	}
}
