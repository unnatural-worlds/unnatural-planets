#include <cage-core/files.h>

#include "mesh.h"

void saveDebugMesh(const string &path, const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving debug mesh: " + path);
	OPTICK_EVENT();

	PolyhedronObjExportConfig cfg;
	cfg.objectName = pathExtractFilenameNoExtension(path);
	mesh->exportObjFile(cfg, path);
}

void saveRenderMesh(const string &path, const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving render mesh: " + path);
	OPTICK_EVENT();

	PolyhedronObjExportConfig cfg;
	cfg.objectName = pathExtractFilenameNoExtension(path);
	cfg.materialLibraryName = cfg.objectName + ".mtl";
	cfg.materialName = cfg.objectName;
	mesh->exportObjFile(cfg, path);

	const string directory = pathExtractPath(path);
	const string cpmName = cfg.objectName + ".cpm";

	{ // write mtl file with link to albedo texture
		Holder<File> f = writeFile(pathJoin(directory, cfg.materialLibraryName));
		f->writeLine(stringizer() + "newmtl " + cfg.materialName);
		f->writeLine(stringizer() + "map_Kd " + cfg.objectName + "-albedo.png");
		f->writeLine(stringizer() + "map_bump " + cfg.objectName + "-height.png");
	}

	{ // write cpm material file
		Holder<File> f = newFile(pathJoin(directory, cpmName), FileMode(false, true));
		f->writeLine("[textures]");
		f->writeLine(stringizer() + "albedo = " + cfg.objectName + "-albedo.png");
		f->writeLine(stringizer() + "special = " + cfg.objectName + "-special.png");
		f->writeLine(stringizer() + "normal = " + cfg.objectName + "-height.png");
	}
}

void saveNavigationMesh(const string &path, const Holder<Polyhedron> &mesh, const std::vector<uint8> &terrainTypes)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving navigation mesh: " + path);
	OPTICK_EVENT();

	CAGE_ASSERT(terrainTypes.size() == mesh->verticesCount());
	Holder<Polyhedron> m = mesh->copy();
	std::vector<vec2> uvs;
	uvs.reserve(terrainTypes.size());
	for (uint8 t : terrainTypes)
		uvs.push_back(vec2((t + 0.5) / 32, 0));
	m->uvs(uvs);

	PolyhedronObjExportConfig cfg;
	cfg.objectName = "navigation";
	m->exportObjFile(cfg, path);
}

void saveCollider(const string &path, const Holder<Polyhedron> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving collider: " + path);
	OPTICK_EVENT();

	Holder<Polyhedron> m = mesh->copy();
	m->normals({});
	m->uvs({});
	PolyhedronObjExportConfig cfg;
	cfg.objectName = "collider";
	m->exportObjFile(cfg, path);
}

real meshAverageEdgeLength(const Holder<Polyhedron> &mesh)
{
	real len = 0;
	const uint32 trisCnt = mesh->indicesCount() / 3;
	for (uint32 tri = 0; tri < trisCnt; tri++)
	{
		vec3 a = mesh->position(mesh->index(tri * 3 + 0));
		vec3 b = mesh->position(mesh->index(tri * 3 + 1));
		vec3 c = mesh->position(mesh->index(tri * 3 + 2));
		len += distance(a, b);
		len += distance(b, c);
		len += distance(c, a);
	}
	return len / mesh->indicesCount();
}




