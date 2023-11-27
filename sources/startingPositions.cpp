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
	void filterStartingPositionsByDoodads(std::vector<uint32> &positions);

	namespace
	{
		std::pair<Real, Real> findMeanAndDev(PointerRange<const Real> values)
		{
			Real avg = 0;
			for (Real s : values)
				avg += s;
			avg /= values.size();
			Real dev = 0;
			for (Real s : values)
				dev += abs(s - avg);
			dev /= values.size();
			return { avg, dev };
		}

		Real scorePositionsEquidistance(PointerRange<const uint32> positions)
		{
			std::vector<Real> dists;
			dists.reserve((positions.size() + 1) * positions.size() / 2);
			for (uint32 i : positions)
				for (uint32 j : positions)
					if (j > i)
						dists.push_back(distance(tiles[i].position, tiles[j].position));
			const auto mad = findMeanAndDev(dists);
			const Real score = (positions.size() + 1) * (mad.first + 100) / (mad.second + 200);
			CAGE_LOG_DEBUG(SeverityEnum::Info, "generator", Stringizer() + "count: " + positions.size() + ", avg: " + mad.first + ", dev: " + mad.second + ", score: " + score);
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
					return b < spatQuery->result().size() * 65 / 100; // eliminate candidates that have buildable less than 65 % surrounding area
				});
		}

		std::vector<uint32> generateCadidates()
		{
			std::vector<uint32> candidates;
			candidates.reserve(tiles.size() / 2);
			for (auto it : enumerate(tiles))
				if (it->buildable)
					candidates.push_back(it.index);
			const uint32 a = numeric_cast<uint32>(candidates.size());
			filterPositionsByBuildableRadius(candidates);
			const uint32 b = numeric_cast<uint32>(candidates.size());
			CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "remaining " + b + " candidates, after eliminating " + (a - b) + " due to insufficient buildable neighbors");
			filterStartingPositionsByDoodads(candidates);
			const uint32 c = numeric_cast<uint32>(candidates.size());
			CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "remaining " + c + " candidates, after eliminating " + (b - c) + " due to doodads");
			return candidates;
		}

		std::vector<uint32> proposeSolution(const std::vector<uint32> &candidates)
		{
			const uint32 cnt = min(randomRange(5u, 20u), numeric_cast<uint32>(candidates.size()));
			FlatSet<uint32> proposal;
			proposal.reserve(cnt);
			uint32 attempt = 0;
			while (proposal.size() < cnt && attempt++ < 1000)
			{
				const uint32 p = candidates[randomRange(std::size_t{}, candidates.size())];
				bool valid = true;
				for (uint32 a : proposal)
					if (distance(tiles[p].position, tiles[a].position) < 600)
						valid = false; // starting positions too close to each other
				if (valid)
					proposal.insert(p);
			}
			return std::move(proposal.unsafeData());
		}
	}

	void generateStartingPositions()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating starting positions");

		const std::vector<uint32> allCandidates = generateCadidates();

		std::vector<uint32> bestSolution;
		Real bestScore = -Real::Infinity();
		static constexpr uint32 Limit = 100000 / (CAGE_DEBUG_BOOL ? 100 : 1);
		for (uint32 attempt = 0; attempt < Limit; attempt++)
		{
			std::vector<uint32> proposal = proposeSolution(allCandidates);
			if (proposal.size() < 3)
				continue;
			const Real score = scorePositionsEquidistance(proposal);
			if (score > bestScore)
			{
				bestSolution = proposal;
				bestScore = score;
			}
		}
		if (bestSolution.empty())
			CAGE_THROW_ERROR(Exception, "generated no starting positions");

		{
			Holder<File> f = writeFile(pathJoin(baseDirectory, "starts.ini"));
			f->writeLine("[]");
			for (uint32 c : bestSolution)
				f->writeLine(Stringizer() + tiles[c].position);
			f->close();
		}

		{
			Holder<Mesh> msh = newMesh();
			for (uint32 h : bestSolution)
			{
				const Vec3 o = tiles[h].position;
				const Vec3 u = tiles[h].normal;
				const Vec3 s = anyPerpendicular(u);
				const Vec3 t = cross(s, u);
				const Vec3 a = o + u * 200 + s * 31;
				const Vec3 b = o + u * 200 - s * 20 + t * 30;
				const Vec3 c = o + u * 200 - s * 20 - t * 30;
				msh->addTriangle(Triangle(o, a, b));
				msh->addTriangle(Triangle(o, b, c));
				msh->addTriangle(Triangle(o, c, a));
				msh->addTriangle(Triangle(a, c, b));
			}
			msh->exportFile(pathJoin(baseDirectory, "starts-preview.obj"));
		}

		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "generated " + bestSolution.size() + " starting positions");
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "starting positions score: " + bestScore);
	}
}
