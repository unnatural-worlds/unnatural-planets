#include <cage-core/files.h>

#include "mesh.h"

namespace
{
	string v2s(const vec3 &v)
	{
		return stringizer() + v[0] + " " + v[1] + " " + v[2];
	}

	string v2s(const vec2 &v)
	{
		return stringizer() + v[0] + " " + v[1];
	}

	void writeIndices(const Holder<File> &f, const Holder<UPMesh> &mesh)
	{
		uint32 cnt = numeric_cast<uint32>(mesh->indices.size()) / 3;
		for (uint32 i = 0; i < cnt; i++)
		{
			string s = "f ";
			for (uint32 j = 0; j < 3; j++)
			{
				uint32 k = mesh->indices[i * 3 + j] + 1;
				s += stringizer() + k + "/" + k + "/" + k + " ";
			}
			f->writeLine(s);
		}
	}
}

Holder<UPMesh> newUPMesh()
{
	return detail::systemArena().createHolder<UPMesh>();
}

void UPMesh::validate() const
{
	if (!normals.empty() && normals.size() != positions.size())
		CAGE_THROW_ERROR(Exception, "invalid count of normals");
	if (!uvs.empty() && uvs.size() != positions.size())
		CAGE_THROW_ERROR(Exception, "invalid count of uvs");
	if ((indices.size() % 3) != 0)
		CAGE_THROW_ERROR(Exception, "indices not divisible by 3");
	for (const vec3 &p : positions)
		if (!p.valid())
			CAGE_THROW_ERROR(Exception, "invalid vertex position");
	for (const vec3 &n : normals)
	{
		if (!n.valid())
			CAGE_THROW_ERROR(Exception, "invalid vertex normal");
		if (abs(length(n) - 1) > 1e-4)
			CAGE_THROW_ERROR(Exception, "invalid normal length");
	}
	for (const vec2 &u : uvs)
		if (!u.valid())
			CAGE_THROW_ERROR(Exception, "invalid vertex uvs");
	for (auto i : indices)
		if (i >= positions.size())
			CAGE_THROW_ERROR(Exception, "invalid mesh index");
}

void saveDebugMesh(const string &path, const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving debug mesh: " + path);
	OPTICK_EVENT();

	string objectName = pathExtractFilenameNoExtension(path);
	Holder<File> f = newFile(path, FileMode(false, true));
	f->writeLine(stringizer() + "o " + objectName);
	for (const vec3 &v : mesh->positions)
		f->writeLine(stringizer() + "v " + v2s(v));
	for (const vec3 &v : mesh->normals)
		f->writeLine(stringizer() + "vn " + v2s(v));
	for (const vec2 &v : mesh->uvs)
		f->writeLine(stringizer() + "vt " + v2s(v));
	writeIndices(f, mesh);
}

void saveRenderMesh(const string &path, const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving render mesh: " + path);
	OPTICK_EVENT();

	string directory = pathExtractPath(path);
	string objectName = pathExtractFilenameNoExtension(path);
	string mtlName = objectName + ".mtl";
	string cpmName = objectName + ".cpm";

	{
		Holder<File> f = newFile(path, FileMode(false, true));
		f->writeLine(stringizer() + "mtllib " + mtlName);
		f->writeLine(stringizer() + "o " + objectName);
		f->writeLine(stringizer() + "usemtl " + objectName);
		for (const vec3 &v : mesh->positions)
			f->writeLine(stringizer() + "v " + v2s(v));
		for (const vec3 &v : mesh->normals)
			f->writeLine(stringizer() + "vn " + v2s(v));
		for (const vec2 &v : mesh->uvs)
			f->writeLine(stringizer() + "vt " + v2s(v));
		writeIndices(f, mesh);
	}

	{ // write mtl file with link to albedo texture
		Holder<File> f = newFile(pathJoin(directory, mtlName), FileMode(false, true));
		f->writeLine(stringizer() + "newmtl " + objectName);
		f->writeLine(stringizer() + "map_Kd " + objectName + "-albedo.png");
		f->writeLine(stringizer() + "map_bump " + objectName + "-height.png");
	}

	{ // write cpm material file
		Holder<File> f = newFile(pathJoin(directory, cpmName), FileMode(false, true));
		f->writeLine("[textures]");
		f->writeLine(stringizer() + "albedo = " + objectName + "-albedo.png");
		f->writeLine(stringizer() + "special = " + objectName + "-special.png");
		f->writeLine(stringizer() + "normal = " + objectName + "-height.png");
	}
}

void saveNavigationMesh(const string &path, const Holder<UPMesh> &mesh, const std::vector<uint8> &terrainTypes)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving navigation mesh: " + path);
	OPTICK_EVENT();

	Holder<File> f = newFile(path, FileMode(false, true));
	f->writeLine("o navigation");
	for (const vec3 &v : mesh->positions)
		f->writeLine(stringizer() + "v " + v2s(v));
	for (const vec3 &v : mesh->normals)
		f->writeLine(stringizer() + "vn " + v2s(v));
	for (const uint8 &v : terrainTypes)
		f->writeLine(stringizer() + "vt " + v2s(vec2((v + 0.5) / 32, 0)));
	writeIndices(f, mesh);
}

void saveCollider(const string &path, const Holder<UPMesh> &mesh)
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "saving collider: " + path);
	OPTICK_EVENT();

	Holder<File> f = newFile(path, FileMode(false, true));
	f->writeLine("o collider");
	for (const vec3 &v : mesh->positions)
		f->writeLine(stringizer() + "v " + v2s(v));
	writeIndices(f, mesh);
}

real meshAverageEdgeLength(const Holder<UPMesh> &mesh)
{
	real len = 0;
	uint32 trisCnt = mesh->indices.size() / 3;
	for (uint32 tri = 0; tri < trisCnt; tri++)
	{
		vec3 a = mesh->positions[mesh->indices[tri * 3 + 0]];
		vec3 b = mesh->positions[mesh->indices[tri * 3 + 1]];
		vec3 c = mesh->positions[mesh->indices[tri * 3 + 2]];
		len += distance(a, b);
		len += distance(b, c);
		len += distance(c, a);
	}
	return len / mesh->indices.size();
}




