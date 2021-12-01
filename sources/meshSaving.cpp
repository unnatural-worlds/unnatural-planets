#include <cage-core/files.h>
#include <cage-core/mesh.h>

#include "terrain.h"
#include "mesh.h"

void meshSaveDebug(const String &path, const Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving debug mesh: " + path);

	MeshExportObjConfig cfg;
	cfg.objectName = pathExtractFilenameNoExtension(path);
	mesh->exportObjFile(path, cfg);
}

void meshSaveRender(const String &path, const Holder<Mesh> &mesh, bool transparency)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving render mesh: " + path);

	CAGE_ASSERT(mesh->normals().size() == mesh->verticesCount());
	CAGE_ASSERT(mesh->uvs().size() == mesh->verticesCount());
	MeshExportObjConfig cfg;
	cfg.objectName = pathExtractFilenameNoExtension(path);
	cfg.materialLibraryName = cfg.objectName + ".mtl";
	cfg.materialName = cfg.objectName;
	mesh->exportObjFile(path, cfg);

	const String directory = pathExtractDirectory(path);
	const String cpmName = cfg.objectName + ".cpm";

	{ // write mtl file with link to albedo texture
		Holder<File> f = writeFile(pathJoin(directory, cfg.materialLibraryName));
		f->writeLine(Stringizer() + "newmtl " + cfg.materialName);
		f->writeLine(Stringizer() + "map_Kd " + cfg.objectName + "-albedo.png");
		
		if (transparency)
			f->writeLine(Stringizer() + "map_d " + cfg.objectName + "-albedo.png");
	}

	{ // write cpm material file
		Holder<File> f = newFile(pathJoin(directory, cpmName), FileMode(false, true));
		f->writeLine("[textures]");
		f->writeLine(Stringizer() + "albedo = " + cfg.objectName + "-albedo.png");
		f->writeLine(Stringizer() + "special = " + cfg.objectName + "-special.png");
		f->writeLine(Stringizer() + "normal = " + cfg.objectName + "-height.png");
		if (transparency)
		{
			f->writeLine("[flags]");
			//f->writeLine("noShadowCast");
			f->writeLine("translucent");
		}
	}
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
	m->exportObjFile(path, cfg);
}

void meshSaveCollider(const String &path, const Holder<Mesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "saving collider: " + path);

	Holder<Mesh> m = mesh->copy();
	m->normals({});
	m->uvs({});
	MeshExportObjConfig cfg;
	cfg.objectName = "collider";
	m->exportObjFile(path, cfg);
}
