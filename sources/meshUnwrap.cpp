#include "mesh.h"

#include <xatlas.h>

#include <cstdarg>
#include <unordered_map>

namespace
{
	int xAtlasPrint(const char *format, ...)
	{
		char buffer[1000];
		va_list arg;
		va_start(arg, format);
		auto result = vsprintf(buffer, format, arg);
		va_end(arg);
		CAGE_LOG_DEBUG(SeverityEnum::Warning, "xatlas", buffer);
		return result;
	}

	bool xAtlasProgress(xatlas::ProgressCategory::Enum category, int progress, void *userData)
	{
		CAGE_LOG(SeverityEnum::Info, "xatlas", stringizer() + "category: " + category + ", progress: " + progress);
		return true; // continue processing
	}

	void destroyAtlas(void *ptr)
	{
		xatlas::Destroy((xatlas::Atlas*)ptr);
	}

	Holder<xatlas::Atlas> newAtlas()
	{
		xatlas::SetPrint(&xAtlasPrint, false);
		xatlas::Atlas *a = xatlas::Create();
		xatlas::SetProgressCallback(a, &xAtlasProgress);
		return Holder<xatlas::Atlas>(a, a, Delegate<void(void*)>().bind<&destroyAtlas>());
	}

	#ifdef CAGE_DEBUG
	const real texelsPerSegment = 1;
	#else
	const real texelsPerSegment = 10;
	#endif // CAGE_DEBUG
}

UnwrapResult meshUnwrap(const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "unwrapping mesh");
	OPTICK_EVENT();

	Holder<xatlas::Atlas> atlas = newAtlas();

	{
		OPTICK_EVENT("AddMesh");
		xatlas::MeshDecl decl;
		decl.indexCount = numeric_cast<uint32>(mesh->indices.size());
		decl.indexData = mesh->indices.data();
		decl.indexFormat = xatlas::IndexFormat::UInt32;
		decl.vertexCount = numeric_cast<uint32>(mesh->positions.size());
		decl.vertexPositionData = mesh->positions.data();
		decl.vertexNormalData = mesh->normals.data();
		decl.vertexPositionStride = sizeof(vec3);
		decl.vertexNormalStride = sizeof(vec3);
		xatlas::AddMesh(atlas.get(), decl);
	}

	{
		OPTICK_EVENT("ComputeCharts");
		xatlas::ChartOptions chart;
		xatlas::ComputeCharts(atlas.get(), chart);
	}

	{
		OPTICK_EVENT("ParameterizeCharts");
		xatlas::ParameterizeCharts(atlas.get());
	}

	{
		OPTICK_EVENT("PackCharts");
		xatlas::PackOptions pack;
		pack.resolution = 512;
		pack.texelsPerUnit = texelsPerSegment.value;
		pack.padding = 2;
		pack.bilinear = true;
		pack.blockAlign = true;
		xatlas::PackCharts(atlas.get(), pack);
		CAGE_ASSERT(atlas->meshCount == 1);
		CAGE_ASSERT(atlas->atlasCount > 0);
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "produced " + atlas->atlasCount + " atlases");
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "textures resolution: " + atlas->width + "x" + atlas->height);
	}

	{
		OPTICK_EVENT("apply");
		UnwrapResult result;
		result.textureWidth = atlas->width;
		result.textureHeight = atlas->height;
		const vec2 whInv = 1 / vec2(atlas->width - 1, atlas->height - 1);
		result.meshes.reserve(atlas->atlasCount);
		xatlas::Mesh *m = atlas->meshes;
		for (uint32 atlasIndex = 0; atlasIndex < atlas->atlasCount; atlasIndex++)
		{
			 Holder<UPMesh> res = newUPMesh();
			 res->positions.reserve(m->vertexCount / atlas->atlasCount);
			 res->normals.reserve(m->vertexCount / atlas->atlasCount);
			 res->uvs.reserve(m->vertexCount / atlas->atlasCount);
			 res->indices.reserve(m->indexCount / atlas->atlasCount);
			 result.meshes.push_back(templates::move(res));
		}
		std::unordered_map<uint32, uint32> mapping;
		for (uint32 indexIndex = 0; indexIndex < m->indexCount; indexIndex++)
		{
			uint32 index = m->indexArray[indexIndex];
			const xatlas::Vertex &v = m->vertexArray[index];
			CAGE_ASSERT(v.atlasIndex >= 0 && v.atlasIndex < atlas->atlasCount);
			Holder<UPMesh> &res = result.meshes[v.atlasIndex];
			if (mapping.count(index) == 0)
			{
				mapping[index] = res->positions.size();
				res->positions.push_back(mesh->positions[v.xref]);
				res->normals.push_back(mesh->normals[v.xref]);
				res->uvs.push_back(vec2(v.uv[0], v.uv[1]) * whInv);
			}
			res->indices.push_back(mapping[index]);
		}
		for (const auto &it : result.meshes)
		{
			CAGE_ASSERT((it->indices.size() % 3) == 0);
		}
		return result;
	}
}



