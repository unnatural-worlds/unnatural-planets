#include <cage-core/files.h>
#include <cage-core/polyhedron.h>

#include "terrain.h"
#include "mesh.h"

void meshSaveDebug(const string &path, const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving debug mesh: " + path);

	PolyhedronObjExportConfig cfg;
	cfg.objectName = pathExtractFilenameNoExtension(path);
	mesh->exportObjFile(cfg, path);
}

void meshSaveRender(const string &path, const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving render mesh: " + path);

	CAGE_ASSERT(mesh->normals().size() == mesh->verticesCount());
	CAGE_ASSERT(mesh->uvs().size() == mesh->verticesCount());
	PolyhedronObjExportConfig cfg;
	cfg.objectName = pathExtractFilenameNoExtension(path);
	cfg.materialLibraryName = cfg.objectName + ".mtl";
	cfg.materialName = cfg.objectName;
	mesh->exportObjFile(cfg, path);

	const string directory = pathExtractDirectory(path);
	const string cpmName = cfg.objectName + ".cpm";

	{ // write mtl file with link to albedo texture
		Holder<File> f = writeFile(pathJoin(directory, cfg.materialLibraryName));
		f->writeLine(stringizer() + "newmtl " + cfg.materialName);
		f->writeLine(stringizer() + "map_Kd " + cfg.objectName + "-albedo.png");
		//f->writeLine(stringizer() + "map_bump " + cfg.objectName + "-height.png");
	}

	{ // write cpm material file
		Holder<File> f = newFile(pathJoin(directory, cpmName), FileMode(false, true));
		f->writeLine("[textures]");
		f->writeLine(stringizer() + "albedo = " + cfg.objectName + "-albedo.png");
		f->writeLine(stringizer() + "special = " + cfg.objectName + "-special.png");
		f->writeLine(stringizer() + "normal = " + cfg.objectName + "-height.png");
	}
}

void meshSaveNavigation(const string &path, const Holder<Polyhedron> &mesh, const std::vector<Tile> &tiles)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving navigation mesh: " + path);

	CAGE_ASSERT(mesh->normals().size() == mesh->verticesCount());
	CAGE_ASSERT(tiles.size() == mesh->verticesCount());
	Holder<Polyhedron> m = mesh->copy();
	std::vector<vec2> uvs;
	uvs.reserve(tiles.size());
	for (const Tile &t : tiles)
	{
		static_assert((uint8)TerrainTypeEnum::_Total <= 32);
		uvs.push_back(vec2(((uint8)(t.type) + 0.5) / 32, 0));
	}
	m->uvs(uvs);

	PolyhedronObjExportConfig cfg;
	cfg.objectName = "navigation";
	m->exportObjFile(cfg, path);
}

void meshSaveCollider(const string &path, const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving collider: " + path);

	Holder<Polyhedron> m = mesh->copy();
	m->normals({});
	m->uvs({});
	PolyhedronObjExportConfig cfg;
	cfg.objectName = "collider";
	m->exportObjFile(cfg, path);
}
