#include <cage-core/files.h>
#include <cage-core/mesh.h>
#include <cage-core/meshExport.h>

#include "terrain.h"
#include "mesh.h"

void meshSaveDebug(const String &path, const Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving debug mesh: " + path);

	MeshExportGltfConfig cfg;
	cfg.name = pathExtractFilenameNoExtension(path);
	cfg.mesh = +mesh;
	meshExportFiles(path, cfg);
}

void meshSaveRender(const String &path, const Holder<Mesh> &mesh, bool transparency)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving render mesh: " + path);

	CAGE_ASSERT(mesh->normals().size() == mesh->verticesCount());
	CAGE_ASSERT(mesh->uvs().size() == mesh->verticesCount());
	MeshExportGltfConfig cfg;
	cfg.name = pathExtractFilenameNoExtension(path);
	cfg.mesh = +mesh;
	cfg.albedo.filename = Stringizer() + cfg.name + "-albedo.png";
	cfg.pbr.filename = Stringizer() + cfg.name + "-pbr.png";
	cfg.normal.filename = Stringizer() + cfg.name + "-normal.png";
	if (transparency)
		cfg.renderFlags |= MeshRenderFlags::Transparent;
	meshExportFiles(path, cfg);
}

void meshSaveNavigation(const String &path, const Holder<Mesh> &mesh, const std::vector<Tile> &tiles)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving navigation mesh: " + path);

	CAGE_ASSERT(mesh->normals().size() == mesh->verticesCount());
	CAGE_ASSERT(tiles.size() == mesh->verticesCount());
	Holder<Mesh> m = mesh->copy();
	std::vector<Vec2> uvs;
	uvs.reserve(tiles.size());
	for (const Tile &t : tiles)
	{
		static_assert((uint8)TerrainTypeEnum::_Total <= 32);
		uvs.push_back(Vec2(((uint8)(t.type) + 0.5) / 32, 0));
	}
	m->uvs(uvs);

	MeshExportObjConfig cfg;
	cfg.objectName = "navigation";
	cfg.mesh = +m;
	meshExportFiles(path, cfg);
}

void meshSaveCollider(const String &path, const Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving collider: " + path);

	Holder<Mesh> m = mesh->copy();
	m->normals({});
	m->uvs({});
	MeshExportGltfConfig cfg;
	cfg.name = "collider";
	cfg.mesh = +m;
	meshExportFiles(path, cfg);
}
