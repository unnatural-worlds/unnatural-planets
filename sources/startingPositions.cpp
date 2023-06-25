#include <cage-core/files.h>
#include <cage-core/geometry.h>
#include <cage-core/mesh.h>

#include "generator.h"
#include "tile.h"

#include <algorithm>
#include <random>

namespace
{
	Real calculateScore(const std::vector<Tile> &tiles, PointerRange<const uint32> positions)
	{
		Real totalDist, minDist = Real::Infinity(), maxDist = -Real::Infinity();
		uint32 distCount = 0;
		for (uint32 i : positions)
		{
			for (uint32 j : positions)
			{
				if (j > i)
				{
					const Real d = distance(tiles[i].position, tiles[j].position);
					if (d < 250)
						return -Real::Infinity(); // starting positions too close to each other
					totalDist += d;
					minDist = min(minDist, d);
					maxDist = max(maxDist, d);
					distCount++;
				}
			}
		}
		const Real avgDist = totalDist / distCount;
		Real devSum;
		for (uint32 i : positions)
			for (uint32 j : positions)
				if (j > i)
					devSum += abs(distance(tiles[i].position, tiles[j].position) - avgDist);
		return minDist * positions.size() - devSum / positions.size();
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

	for (uint32 attempt = 0; attempt < 200; attempt++)
	{
		std::vector<uint32> current = pickRandomPositions(tiles);
		Real score = calculateScore(tiles, current);
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
}
