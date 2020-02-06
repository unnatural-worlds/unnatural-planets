#include "generator.h"
#include "functions.h"

#include <dualmc.h>

namespace
{
	std::vector<real> genDensities(real size, uint32 resolution)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating densities");
		OPTICK_EVENT();

		std::vector<real> densities;
		densities.reserve(resolution * resolution * resolution);
		for (uint32 z = 0; z < resolution; z++)
		{
			for (uint32 y = 0; y < resolution; y++)
			{
				for (uint32 x = 0; x < resolution; x++)
				{
					vec3 p = (vec3(x, y, z) / (resolution - 1) - 0.5) * size;
					real d = functionDensity(p);
					CAGE_ASSERT(d.valid());
					densities.push_back(d);
				}
			}
		}
		return densities;
	}

	void genSurface(const std::vector<real> &densities, uint32 resolution, std::vector<dualmc::Vertex> &mcVertices, std::vector<dualmc::Quad> &mcIndices)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating surface");
		OPTICK_EVENT();

		dualmc::DualMC<float> mc;
		mc.build((float*)densities.data(), resolution, resolution, resolution, 0, true, false, mcVertices, mcIndices);
		//CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "vertices count: " + mcVertices.size() + ", quads count: " + mcIndices.size());
		if (mcVertices.size() == 0 || mcIndices.size() == 0)
			CAGE_THROW_ERROR(Exception, "generated empty mesh");
	}

	Holder<UPMesh> genTriangles(const std::vector<dualmc::Vertex> &mcVertices, const std::vector<dualmc::Quad> &mcIndices, real size, uint32 resolution)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating triangles");
		OPTICK_EVENT();

		Holder<UPMesh> result = newUPMesh();
		result->positions.reserve(mcVertices.size());
		result->normals.resize(mcVertices.size());
		for (const dualmc::Vertex &v : mcVertices)
		{
			vec3 p = (vec3(v.x, v.y, v.z) / (resolution - 1) - 0.5) * size;
			result->positions.push_back(p);
		}
		result->indices.reserve(mcIndices.size() * 6 / 4);
		for (const auto &q : mcIndices)
		{
			const uint32 is[4] = { numeric_cast<uint32>(q.i0), numeric_cast<uint32>(q.i1), numeric_cast<uint32>(q.i2), numeric_cast<uint32>(q.i3) };
#define P(I) result->positions[is[I]]
			bool which = distanceSquared(P(0), P(2)) < distanceSquared(P(1), P(3)); // split the quad by shorter diagonal
#undef P
			static const int first[6] = { 0,1,2, 0,2,3 };
			static const int second[6] = { 1,2,3, 1,3,0 };
			for (uint32 i : (which ? first : second))
				result->indices.push_back(is[i]);
			vec3 n;
			{
				vec3 a = result->positions[is[0]];
				vec3 b = result->positions[is[1]];
				vec3 c = result->positions[is[2]];
				n = normalize(cross(b - a, c - a));
			}
			for (uint32 i : is)
				result->normals[i] += n;
		}
		for (auto &it : result->normals)
			it = normalize(it);
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "vertices count: " + result->positions.size() + ", triangles count: " + result->indices.size() / 3);
		return result;
	}
}

Holder<UPMesh> generateBaseMesh(real size, uint32 resolution)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base mesh");
	OPTICK_EVENT();

	std::vector<real> densities = genDensities(size, resolution);
	std::vector<dualmc::Vertex> mcVertices;
	std::vector<dualmc::Quad> mcIndices;
	genSurface(densities, resolution, mcVertices, mcIndices);
	Holder<UPMesh> res = genTriangles(mcVertices, mcIndices, size, resolution);

	{
		real len = 0;
		uint32 trisCnt = res->indices.size() / 3;
		for (uint32 tri = 0; tri < trisCnt; tri++)
		{
			vec3 a = res->positions[res->indices[tri * 3 + 0]];
			vec3 b = res->positions[res->indices[tri * 3 + 1]];
			vec3 c = res->positions[res->indices[tri * 3 + 2]];
			len += distance(a, b);
			len += distance(b, c);
			len += distance(c, a);
		}
		real avg = len / res->indices.size();
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "average edge length: " + avg);
	}
	return res;
}





