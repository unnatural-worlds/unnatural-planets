#include "generator.h"

#include <cage-core/filesystem.h>
#include <cage-core/utility/noise.h>
#include <cage-core/concurrent.h>

namespace generator
{
	meshStruct::meshStruct() : collider(false)
	{}

	void meshStruct::clear()
	{
		triangles.clear();
		albedo.clear();
		special.clear();
	}

	void meshStruct::insideOut()
	{
		for (triangleStruct &t : triangles)
			std::swap(t.coordinates[1], t.coordinates[2]);
	}

	void meshStruct::flatShading()
	{
		for (triangleStruct &t : triangles)
			t.normals[0] = t.normals[1] = t.normals[2] = t.coordinates.normal();
	}

	void meshStruct::smoothShading()
	{
		// todo
		flatShading();
	}

	aabb meshStruct::box() const
	{
		aabb r;
		for (auto &t : triangles)
			r += aabb(t.coordinates);
		return r;
	}

	namespace
	{
		string v2s(const vec3 &v)
		{
			return string() + v[0] + " " + v[1] + " " + v[2];
		}

		string v2s(const vec2 &v)
		{
			return string() + v[0] + " " + v[1];
		}

		string i2s(uint32 i)
		{
			return string() + i + "/" + i + "/" + i;
		}

		void saveObj(meshStruct &mesh)
		{
			holder<fileClass> f = newFile(string() + globalSeed + "/" + mesh.name + ".obj", fileMode(false, true, true));
			uint32 i = 1;
			for (const triangleStruct &t : mesh.triangles)
			{
				for (uint32 j = 0; j < 3; j++)
				{
					f->writeLine(string() + "v " + v2s(t.coordinates[j]));
					f->writeLine(string() + "vn " + v2s(t.normals[j]));
					f->writeLine(string() + "vt " + v2s(t.uvs[j]));
				}
				f->writeLine(string() + "f " + i2s(i) + " " + i2s(i + 1) + " " + i2s(i + 2));
				i += 3;
			}
		}

		void saveMat(meshStruct &mesh)
		{
			holder<fileClass> f = newFile(string() + globalSeed + "/" + mesh.name + ".obj_DefaultMaterial.cpm", fileMode(false, true, true));
			f->writeLine("[textures]");
			if (mesh.albedo)
				f->writeLine(string() + "albedo = " + mesh.name + "_albedo.png");
			if (mesh.special)
				f->writeLine(string() + "special = " + mesh.name + "_special.png");
			f->writeLine(mesh.material);
		}

		holder<mutexClass> sharedFilesMut = newMutex();
	}

	void meshStruct::save()
	{
		if (triangles.size() == 0)
			return;

		CAGE_LOG(severityEnum::Info, "generator", string() + "saving mesh '" + name + "'");

		saveObj(*this);
		saveMat(*this);

		// save textures
		if (albedo)
			albedo->encodeFile(string() + globalSeed + "/" + name + "_albedo.png");
		if (special)
			special->encodeFile(string() + globalSeed + "/" + name + "_special.png");

		// make object
		{
			holder<fileClass> f = newFile(string() + globalSeed + "/" + name + ".object", fileMode(false, true, true));
			f->writeLine("[]");
			f->writeLine(name + ".obj");
			if (collider)
			{
				f->writeLine("[ref]");
				f->writeLine(string() + "collider = " + name + ".obj;collider");
			}
		}

		// access shared files
		{
			scopeLock<mutexClass> l(sharedFilesMut);

			// add obj to assets pack
			{
				holder<fileClass> f = newFile(packName, fileMode(false, true, true, true));
				f->writeLine(name + ".object");
			}

			// add to assets
			{
				holder<fileClass> f = newFile(assetsName, fileMode(false, true, true, true));
				f->writeLine("[]");
				f->writeLine("scheme = mesh");
				f->writeLine("export_tangent = false");
				f->writeLine(name + ".obj");
				f->writeLine("[]");
				f->writeLine("scheme = object");
				f->writeLine(name + ".object");
				if (albedo)
				{
					f->writeLine("[]");
					f->writeLine("scheme = texture");
					f->writeLine("premultiply_alpha = true");
					f->writeLine(name + "_albedo.png");
				}
				if (special)
				{
					f->writeLine("[]");
					f->writeLine("scheme = texture");
					f->writeLine(name + "_special.png");
				}
				if (collider)
				{
					f->writeLine("[]");
					f->writeLine("scheme = collider");
					f->writeLine(name + ".obj;collider");
				}
			}
		}
	}

	namespace
	{
		real density(const vec3 &p)
		{
			vec3 p1 = p * 0.01;
			vec3 p2 = p * 0.20;
			real roughness = noiseClouds(uint32(globalSeed) + 127, p1, 2);
			return interpolate(0.5, noiseClouds(uint32(globalSeed) + 129, p2, 3), roughness) * 4;
		}
	}

	void makeMeshCave(const graphStruct &graph, uint32 side, const aabb &box, meshStruct &mesh)
	{
		std::vector<real> densities;
		uint32 s = side + 1;
		densities.resize(s * s * s);
		vec3 bs = box.size() / side;
		for (uint32 z = 0; z < s; z++)
		{
			for (uint32 y = 0; y < s; y++)
			{
				for (uint32 x = 0; x < s; x++)
				{
					vec3 p = vec3(x, y, z) * bs + box.a;
					real d = real::PositiveInfinity;
					for (const graphStruct::linkStruct &l : graph.links)
						d = min(d, distance(p, makeSegment(graph.nodes[l.a].position, graph.nodes[l.b].position)) - l.radius);
					for (const graphStruct::nodeStruct &n : graph.nodes)
						d = min(d, distance(p, n.position) - n.radius);
					d += density(p);
					densities[(z * s + y) * s + x] = d;
				}
			}
		}
		mesh.marching(side, densities.data(), box);
	}

	void makeMeshMinimap(const graphStruct &graph, meshStruct &mesh)
	{
		mesh.clear();
		//uint32 nc = numeric_cast<uint32>(graph.nodes.size());
		uint32 lc = numeric_cast<uint32>(graph.links.size());

		mesh.albedo = newPngImage();
		mesh.albedo->empty(lc, 2, 3);

		uint32 li = 0;
		for (const graphStruct::linkStruct &l : graph.links)
		{
			vec3 ap = graph.nodes[l.a].position;
			vec3 bp = graph.nodes[l.b].position;
			triangleStruct t(ap, bp, bp);
			real u = (li + 0.5) / lc;
			t.uvs[0] = vec2(u, 0.25);
			t.uvs[1] = t.uvs[2] = vec2(u, 0.75);
			mesh.triangles.push_back(t);

			vec3 ac = graph.nodes[l.a].color;
			mesh.albedo->value(li, 1, 0, ac[0].value);
			mesh.albedo->value(li, 1, 1, ac[1].value);
			mesh.albedo->value(li, 1, 2, ac[2].value);
			vec3 bc = graph.nodes[l.b].color;
			mesh.albedo->value(li, 0, 0, bc[0].value);
			mesh.albedo->value(li, 0, 1, bc[1].value);
			mesh.albedo->value(li, 0, 2, bc[2].value);

			li++;
		}
	}
}
