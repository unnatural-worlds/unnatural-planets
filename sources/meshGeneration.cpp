#include <cage-core/geometry.h>
#include <cage-core/config.h>
#include <cage-core/mesh.h>
#include <cage-core/marchingCubes.h>
#include <unnatural-navmesh/navmesh.h>

#include "terrain.h"
#include "mesh.h"

#include <initializer_list>

namespace
{
	constexpr real boxSize = 2500;
#ifdef CAGE_DEBUG
	constexpr uint32 boxResolution = 70;
	constexpr uint32 iterations = 1;
	constexpr float tileSize = 30;
#else
	constexpr uint32 boxResolution = 300;
	constexpr uint32 iterations = 10;
	constexpr float tileSize = 10;
#endif // CAGE_DEBUG

	ConfigBool configNavmeshOptimize("unnatural-planets/navmesh/optimize");

	template<real(*FNC)(const vec3 &)>
	Holder<Mesh> meshGenerateGeneric()
	{
		MarchingCubesCreateConfig cfg;
		cfg.box = Aabb(vec3(boxSize * -0.5), vec3(boxSize * 0.5));
		cfg.resolution = ivec3(boxResolution);
		Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
		cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<FNC>());
		Holder<Mesh> poly = cubes->makeMesh();
		meshDiscardDisconnected(+poly);
		meshFlipNormals(+poly);
		return poly;
	}
}

Holder<Mesh> meshGenerateBaseLand()
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base land mesh");
	Holder<Mesh> poly = meshGenerateGeneric<&terrainSdfLand>();
	if (poly->indicesCount() == 0)
		CAGE_THROW_ERROR(Exception, "generated empty base land mesh");
	return poly;
}

Holder<Mesh> meshGenerateBaseWater()
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base water mesh");
	Holder<Mesh> poly = meshGenerateGeneric<&terrainSdfWater>();

	{
		meshConvertToIndexed(+poly);

		// check which vertices are needed
		std::vector<bool> valid;
		valid.reserve(poly->verticesCount());
		for (const vec3 &p : poly->positions())
			valid.push_back(terrainSdfElevationRaw(p) < 0.1);

		// expand valid vertices to whole triangles and their neighbors
		for (uint32 j = 0; j < 2; j++)
		{
			std::vector<bool> valid2 = valid;
			const uint32 cnt = poly->indicesCount();
			const auto inds = poly->indices();
			for (uint32 i = 0; i < cnt; i += 3)
			{
				const uint32 is[3] = { inds[i + 0], inds[i + 1], inds[i + 2] };
				if (valid[is[0]] || valid[is[1]] || valid[is[2]])
					valid[is[0]] = valid[is[1]] = valid[is[2]] = true;
			}
			std::swap(valid, valid2);
		}

		// invalidate unnecessary vertices
		{
			auto v = valid.begin();
			for (vec3 &p : poly->positions())
				if (!*v++)
					p = vec3::Nan();
		}

		meshDiscardInvalid(+poly);
	}

	return poly;
}

Holder<Mesh> meshGenerateBaseNavigation()
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating base navigation mesh");
	Holder<Mesh> poly = meshGenerateGeneric<&terrainSdfNavigation>();
	if (poly->indicesCount() == 0)
		CAGE_THROW_ERROR(Exception, "generated empty base navigation mesh");
	return poly;
}

void meshSimplifyNavmesh(Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing navigation mesh");

	if (configNavmeshOptimize)
	{
		unnatural::NavmeshOptimizeConfig cfg;
#ifdef CAGE_DEBUG
		cfg.iterations = 1;
#endif
		cfg.tileSize = tileSize;
		mesh = unnatural::navmeshOptimize(std::move(mesh), cfg);
	}
	else
	{
		MeshRegularizeConfig cfg;
		cfg.iterations = iterations;
		cfg.targetEdgeLength = tileSize;
		meshRegularize(+mesh, cfg);
	}
}

void meshSimplifyCollider(Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying collider mesh");

	MeshSimplifyConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.5 * tileSize;
	cfg.maxEdgeLength = 10 * tileSize;
	cfg.approximateError = 0.03 * tileSize;
	Holder<Mesh> m = mesh->copy();
	meshSimplify(+m, cfg);

	if (m->indicesCount() <= mesh->indicesCount())
		mesh = std::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified collider mesh has more triangles than the original");
}

void meshSimplifyRender(Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying render mesh");

	MeshSimplifyConfig cfg;
	cfg.iterations = iterations;
	cfg.minEdgeLength = 0.2 * tileSize;
	cfg.maxEdgeLength = 5 * tileSize;
	cfg.approximateError = 0.01 * tileSize;
	Holder<Mesh> m = mesh->copy();
	meshSimplify(+m, cfg);

	if (m->indicesCount() <= mesh->indicesCount())
		mesh = std::move(m);
	else
		CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified render mesh has more triangles than the original");
}

std::vector<Holder<Mesh>> meshSplit(const Holder<Mesh> &mesh)
{
	MeshChunkingConfig cfg;
	cfg.maxSurfaceArea = 250000;
	auto res = meshChunking(+mesh, cfg);
	std::vector<Holder<Mesh>> out;
	out.reserve(res.size());
	for (auto &it : res)
		out.push_back(std::move(it));
	return out;
}

uint32 meshUnwrap(const Holder<Mesh> &mesh)
{
	MeshUnwrapConfig cfg;
	cfg.maxChartIterations = 10;
	cfg.maxChartBoundaryLength = 500;
	cfg.chartRoundness = 0.3;
#ifdef CAGE_DEBUG
	cfg.texelsPerUnit = 0.3;
#else
	cfg.texelsPerUnit = 2.5;
#endif // CAGE_DEBUG
	cfg.padding = 6;
	return meshUnwrap(+mesh, cfg);
}
