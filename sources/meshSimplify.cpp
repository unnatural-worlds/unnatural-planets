#include "mesh.h"

#include <pmp/SurfaceMesh.h>
#include <pmp/algorithms/SurfaceRemeshing.h>

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
	return fromPmp(pm);
}

