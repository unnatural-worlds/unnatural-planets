
#include "main.h"

#include <cage-core/log.h>
#include <cage-core/noise.h>
#include <cage-core/png.h>
#include <cage-core/filesystem.h>
#include <cage-core/memoryBuffer.h>

#include "dualmc.h"

#include <initializer_list>

namespace
{
#ifdef CAGE_DEBUG
	const uint32 verticesPerSide = 70;
	const uint32 texelsPerSegment = 3;
#else
	const uint32 verticesPerSide = 200;
	const uint32 texelsPerSegment = 5;
#endif // CAGE_DEBUG

	const real uvBorderFraction = 0.2;

	struct vertexStruct
	{
		vec3 position;
		vec3 normal;
		vec2 uv;
	};

	// temporary data
	std::vector<dualmc::Vertex> quadVertices; // model space, indexed
	std::vector<dualmc::Quad> quadIndices;
	std::vector<vec3> quadPositions; // world space, NOT indexed
	std::vector<vec3> quadNormals;
	std::vector<float> densities;
	uint32 quadsPerLine;

	// output data
	std::vector<vertexStruct> vertices;
	holder<pngImageClass> albedo;
	holder<pngImageClass> special;

	void genDensities()
	{
		densities.reserve(verticesPerSide * verticesPerSide * verticesPerSide);
		for (uint32 z = 0; z < verticesPerSide; z++)
		{
			for (uint32 y = 0; y < verticesPerSide; y++)
			{
				for (uint32 x = 0; x < verticesPerSide; x++)
				{
					vec3 d = vec3(x, y, z) * 2 / verticesPerSide - 1;
					densities.push_back(terrainDensity(d).value);
				}
			}
		}
	}

	void genSurface()
	{
		dualmc::DualMC<float> mc;
		mc.build(densities.data(), verticesPerSide, verticesPerSide, verticesPerSide, 0, true, false, quadVertices, quadIndices);
	}

	vec3 mc2c(const dualmc::Vertex &v)
	{
		return vec3(v.x, v.y, v.z) * 2 / verticesPerSide - 1;
	}

	void genNormals()
	{
		quadNormals.resize(quadVertices.size());
		for (dualmc::Quad q : quadIndices)
		{
			vec3 p[4] = {
				mc2c(quadVertices[q.i0]),
				mc2c(quadVertices[q.i1]),
				mc2c(quadVertices[q.i2]),
				mc2c(quadVertices[q.i3])
			};
			vec3 n = cross(p[1] - p[0], p[3] - p[0]).normalize();
			for (uint32 i : { q.i0, q.i1, q.i2, q.i3 })
				quadNormals[i] += n;
		}
		for (vec3 &n : quadNormals)
			n = n.normalize();
	}

	void genGeometry()
	{
		CAGE_ASSERT_RUNTIME(vertices.empty());
		vertices.reserve(quadIndices.size() * 6 / 4);
		quadPositions.reserve(vertices.capacity());
		uint32 quadsCount = numeric_cast<uint32>(quadIndices.size());
		quadsPerLine = numeric_cast<uint32>(sqrt(quadsCount));
		if (quadsPerLine * quadsPerLine < quadsCount)
			quadsPerLine++;
		uint32 quadIndex = 0;
		real uvw = real(1) / quadsPerLine;
		for (const auto &q : quadIndices)
		{
			vec3 p[4] = {
				mc2c(quadVertices[q.i0]),
				mc2c(quadVertices[q.i1]),
				mc2c(quadVertices[q.i2]),
				mc2c(quadVertices[q.i3])
			};
			for (const vec3 &pi : p)
				quadPositions.push_back(pi);
			vec3 n[4] = {
				quadNormals[q.i0],
				quadNormals[q.i1],
				quadNormals[q.i2],
				quadNormals[q.i3]
			};
			uint32 qx = quadIndex % quadsPerLine;
			uint32 qy = quadIndex / quadsPerLine;
			vec2 uv[4] = {
				vec2(0, 0),
				vec2(1, 0),
				vec2(1, 1),
				vec2(0, 1)
			};
			for (vec2 &u : uv)
			{
				u = rescale(u, 0, 1, uvBorderFraction, 1 - uvBorderFraction);
				u = (vec2(qx, qy) + u) * uvw;
			}
			for (uint32 i : { 0,1,2,3 })
			{
				vertexStruct v;
				v.position = p[i];
				v.normal = n[i];
				v.uv = uv[i];
				vertices.push_back(v);
			}
			quadIndex++;
		}
	}

	void genTextures()
	{
		uint32 quadsCount = numeric_cast<uint32>(quadPositions.size() / 4);
		uint32 res = quadsPerLine * texelsPerSegment;
		albedo = newPngImage();
		albedo->empty(res, res, 3);
		special = newPngImage();
		special->empty(res, res, 3);
		for (uint32 y = 0; y < res; y++)
		{
			for (uint32 x = 0; x < res; x++)
			{
				uint32 xx = x / texelsPerSegment;
				uint32 yy = y / texelsPerSegment;
				CAGE_ASSERT_RUNTIME(xx < quadsPerLine && yy < quadsPerLine, x, y, xx, yy, texelsPerSegment, quadsPerLine, res);
				uint32 quadIndex = yy * quadsPerLine + xx;
				if (quadIndex >= quadsCount)
					break;
				vec2 f = vec2(x % texelsPerSegment, y % texelsPerSegment) / texelsPerSegment;
				CAGE_ASSERT_RUNTIME(f[0] >= 0 && f[0] <= 1 && f[1] >= 0 && f[1] <= 1, f);
				f = rescale(f, uvBorderFraction, 1 - uvBorderFraction, 0, 1);
				//f = (f - uvBorderFraction) / (1 - uvBorderFraction * 2);
				vec3 pos = interpolate(
					interpolate(quadPositions[quadIndex * 4 + 0], quadPositions[quadIndex * 4 + 1], f[0]),
					interpolate(quadPositions[quadIndex * 4 + 3], quadPositions[quadIndex * 4 + 2], f[0]),
					f[1]);
				vec3 alb, spc;
				terrainMaterial(pos, alb, spc);
				for (uint32 i = 0; i < 3; i++)
					albedo->value(x, y, i, alb[i].value);
				for (uint32 i = 0; i < 3; i++)
					special->value(x, y, i, spc[i].value);
			}
		}
	}
}

void generateTerrain()
{
	// generate mesh
	CAGE_LOG(severityEnum::Info, "generator", string() + "generating densities");
	genDensities();
	CAGE_LOG(severityEnum::Info, "generator", string() + "generating surface");
	genSurface();
	CAGE_LOG(severityEnum::Info, "generator", string() + "generating normals");
	genNormals();
	CAGE_LOG(severityEnum::Info, "generator", string() + "generating geometry");
	genGeometry();
	CAGE_LOG(severityEnum::Info, "generator", string() + "generated mesh with " + vertices.size() + " vertices");

	if (vertices.size() == 0)
		CAGE_THROW_ERROR(exception, "generated empty mesh");

	// generate textures
	CAGE_LOG(severityEnum::Info, "generator", string() + "generating textures");
	genTextures();
	albedo->verticalFlip();
	special->verticalFlip();
	CAGE_LOG(severityEnum::Info, "generator", string() + "generated textures with " + albedo->width() + "*" + albedo->height() + " pixels");
}

void exportTerrain()
{
	holder<filesystemClass> fs = newFilesystem();
	fs->changeDir(string() + "output/" + globalSeed);
	fs->remove("."); // remove previous output

	{ // write file with current seed
		holder<fileClass> f = fs->openFile("info.txt", fileMode(false, true));
		f->writeLine(string() + "seed: " + globalSeed);
	}

	{ // write obj file with geometry
		holder<fileClass> f = fs->openFile("planet.obj", fileMode(false, true));
		f->writeLine("mtllib planet.mtl");
		f->writeLine("o planet");
		f->writeLine("usemtl planet");
		const auto &c = [](const string &s) { return s.replace(",", " ").replace("(", " ").replace(")", " "); };
		for (const vertexStruct &v : vertices)
			f->writeLine(string() + "v " + c(v.position));
		for (const vertexStruct &v : vertices)
			f->writeLine(string() + "vn " + c(v.normal));
		for (const vertexStruct &v : vertices)
			f->writeLine(string() + "vt " + c(v.uv));
		uint32 cnt = vertices.size() / 4;
		for (uint32 i = 0; i < cnt; i++)
		{
			string s = "f ";
			for (uint32 j = 0; j < 4; j++)
			{
				uint32 k = i * 4 + j + 1;
				s += string() + k + "/" + k + "/" + k + " ";
			}
			f->writeLine(s);
		}
	}

	{ // write textures
		fs->openFile("albedo.png", fileMode(false, true))->writeBuffer(albedo->encodeBuffer());
		fs->openFile("special.png", fileMode(false, true))->writeBuffer(special->encodeBuffer());
	}

	{ // write mtl file with link to albedo texture
		holder<fileClass> f = fs->openFile("planet.mtl", fileMode(false, true));
		f->writeLine("newmtl planet");
		f->writeLine("map_Kd albedo.png");
	}

	{ // write cpm material file
		holder<fileClass> f = fs->openFile("planet.cpm", fileMode(false, true));
		f->writeLine("[textures]");
		f->writeLine("albedo = albedo.png");
		f->writeLine("special = special.png");
	}

	{ // object file
		holder<fileClass> f = fs->openFile("planet.object", fileMode(false, true));
		f->writeLine("[]");
		f->writeLine("planet.obj");
	}

	{ // generate asset configuration
		holder<fileClass> f = fs->openFile("planet.asset", fileMode(false, true));
		f->writeLine("[]");
		f->writeLine("scheme = texture");
		f->writeLine("srgb = true");
		f->writeLine("albedo.png");
		f->writeLine("[]");
		f->writeLine("scheme = texture");
		f->writeLine("special.png");
		f->writeLine("[]");
		f->writeLine("scheme = mesh");
		f->writeLine("planet.obj");
		f->writeLine("[]");
		f->writeLine("scheme = object");
		f->writeLine("planet.object");
	}
}
