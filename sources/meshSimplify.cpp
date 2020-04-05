#include "mesh.h"

#include <pmp/SurfaceMesh.h>
#include <pmp/algorithms/SurfaceRemeshing.h>

#include <map>
#include <set>

namespace
{
	Holder<pmp::SurfaceMesh> toPmp(const Holder<UPMesh> &mesh)
	{
		Holder<pmp::SurfaceMesh> res = detail::systemArena().createHolder<pmp::SurfaceMesh>();
		{ // vertices
			for (const vec3 &p : mesh->positions)
				res->add_vertex(pmp::Point(p[0].value, p[1].value, p[2].value));
			pmp::VertexProperty<pmp::Point> normals = res->add_vertex_property<pmp::Point>("v:normal");
			for (const auto &p : mesh->normals)
				normals.vector().push_back(pmp::Point(p[0].value, p[1].value, p[2].value));
		}
		{ // indices
			uint32 tris = numeric_cast<uint32>(mesh->indices.size()) / 3;
			for (uint32 t = 0; t < tris; t++)
				res->add_triangle(pmp::Vertex(mesh->indices[t * 3 + 0]), pmp::Vertex(mesh->indices[t * 3 + 1]), pmp::Vertex(mesh->indices[t * 3 + 2]));
		}
		return res;
	}

	Holder<UPMesh> fromPmp(const Holder<pmp::SurfaceMesh> &pm)
	{
		CAGE_ASSERT(pm->is_triangle_mesh());
		Holder<UPMesh> res = newUPMesh();
		{ // vertices
			for (const auto &v : pm->positions())
				res->positions.push_back(vec3(v[0], v[1], v[2]));
			pmp::VertexProperty<pmp::Point> normals = pm->get_vertex_property<pmp::Point>("v:normal");
			for (auto it = pm->vertices_begin(), et = pm->vertices_end(); it != et; ++it)
			{
				pmp::Point v = normals[*it];
				res->normals.push_back(vec3(v[0], v[1], v[2]));
			}
		}
		// indices
		for (const auto &f : pm->faces())
			for (const auto &i : pm->vertices(f))
				res->indices.push_back(i.idx());
		return res;
	}

#ifdef CAGE_DEBUG
	const uint32 iterations = 1;
	const float targetScale = 3;
#else
	const uint32 iterations = 10;
	const float targetScale = 1;
#endif // CAGE_DEBUG
}

Holder<UPMesh> meshSimplifyRegular(const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "regularizing mesh");
	OPTICK_EVENT();
	Holder<pmp::SurfaceMesh> pm = toPmp(mesh);
	pmp::SurfaceRemeshing rms(*pm);
	rms.uniform_remeshing(1 * targetScale, iterations);
	return fromPmp(pm);
}

Holder<UPMesh> meshSimplifyDynamic(const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "simplifying mesh");
	OPTICK_EVENT();
	Holder<pmp::SurfaceMesh> pm = toPmp(mesh);
	pmp::SurfaceRemeshing rms(*pm);
	rms.adaptive_remeshing(0.5 * targetScale, 10 * targetScale, 0.03 * targetScale, iterations);
	auto result = fromPmp(pm);
	if (result->indices.size() < mesh->indices.size())
		return result;
	CAGE_LOG(SeverityEnum::Warning, "generator", stringizer() + "the simplified mesh has more triangles than the original");
	*result = *mesh; // copy the values
	return result;
}

Holder<UPMesh> meshDiscardDisconnected(Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "detecting disconnected parts");
	OPTICK_EVENT();

	std::map<uint32, std::vector<uint32>> trisByVert;
	{
		uint32 t = 0;
		for (uint32 i : mesh->indices)
			trisByVert[i].push_back(t++ / 3);
	}

	std::vector<uint32> components;
	components.resize(mesh->positions.size(), m);
	uint32 componentsCount = 0;
	{ // assign component index to each vertex
		for (uint32 startPosition = 0; startPosition < mesh->positions.size(); startPosition++)
		{
			if (components[startPosition] != m)
				continue;
			std::set<uint32> open; // open vertices
			open.insert(startPosition);
			components[startPosition] = componentsCount;
			while (!open.empty())
			{
				uint32 v = *open.begin();
				open.erase(v);
				CAGE_ASSERT(components[v] == componentsCount);
				for (uint32 t : trisByVert[v])
				{
					for (uint32 vi = 0; vi < 3; vi++)
					{
						uint32 v = mesh->indices[t * 3 + vi];
						if (components[v] != m)
							continue;
						open.insert(v);
						components[v] = componentsCount;
					}
				}
			}
			componentsCount++;
		}
	}
	if (componentsCount == 1)
		return templates::move(mesh);

	CAGE_LOG(SeverityEnum::Warning, "generator", "found disconnected parts");

	std::vector<uint32> counts;
	counts.resize(componentsCount);
	for (uint32 c : components)
		counts[c]++;
	uint32 largest = 0;
	for (uint32 i = 0; i < componentsCount; i++)
		if (counts[i] > counts[largest])
			largest = i;
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "largest component has: " + counts[largest] + " vertices, discarding " + (mesh->positions.size() - counts[largest]) + " vertices");

	Holder<UPMesh> result = newUPMesh();
	result->positions.reserve(counts[largest]);
	result->normals.reserve(counts[largest]);

	std::map<uint32, uint32> vertRemap;
	{
		uint32 t = 0;
		for (uint32 i = 0; i < mesh->positions.size(); i++)
		{
			if (components[i] == largest)
			{
				result->positions.push_back(mesh->positions[i]);
				result->normals.push_back(mesh->normals[i]);
				vertRemap[i] = t++;
			}
		}
		CAGE_ASSERT(t == counts[largest]);
		CAGE_ASSERT(result->positions.size() == t);
	}

	result->indices.reserve(mesh->indices.size());
	for (uint32 i : mesh->indices)
		if (components[i] == largest)
			result->indices.push_back(vertRemap[i]);

	return result;
}
