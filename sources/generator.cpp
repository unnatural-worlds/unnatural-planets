#include "main.h"

#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/enumerate.h>
#include <cage-core/files.h>
#include <cage-core/fileUtils.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/threadPool.h>
#include <cage-core/timer.h> // formatDateTime
#include <cage-engine/core.h> // ivec2

#include "dualmc.h"
#include "xatlas.h"
#include "ng_mesh.h"
#include <optick.h>

#include <cstdarg>
#include <algorithm>

real planetScale = 1;

namespace
{
	struct Vertex
	{
		vec3 position;
		vec3 normal;
		vec2 uv;
	};

	std::vector<real> densities;
	std::vector<dualmc::Vertex> mcVertices;
	std::vector<dualmc::Quad> mcIndices;
	std::vector<Vertex> meshVertices;
	std::vector<uint32> meshIndices;
	std::vector<vec2> pathProperties;
	Holder<xatlas::Atlas> atlas;
	Holder<Image> albedo;
	Holder<Image> special;
	Holder<Image> heightMap;

#ifdef CAGE_DEBUG
	const uint32 verticesPerSide = 40;
	const real texelsPerSegment = 0.1;
	const uint32 textureUpscale = 1;
#else
	const uint32 verticesPerSide = 200;
	const real texelsPerSegment = 3;
	const uint32 textureUpscale = 4;
#endif // CAGE_DEBUG

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

	inline void destroyAtlas(void *ptr)
	{
		xatlas::Destroy((xatlas::Atlas*)ptr);
	}

	inline Holder<xatlas::Atlas> newAtlas()
	{
		xatlas::SetPrint(&xAtlasPrint, false);
		xatlas::Atlas *a = xatlas::Create();
		xatlas::SetProgressCallback(a, &xAtlasProgress);
		return Holder<xatlas::Atlas>(a, a, Delegate<void(void*)>().bind<&destroyAtlas>());
	}

	inline vec2 barycoord(const triangle &t, const vec2 &p)
	{
		vec2 a = vec2(t[0]);
		vec2 b = vec2(t[1]);
		vec2 c = vec2(t[2]);
		vec2 v0 = b - a;
		vec2 v1 = c - a;
		vec2 v2 = p - a;
		real d00 = dot(v0, v0);
		real d01 = dot(v0, v1);
		real d11 = dot(v1, v1);
		real d20 = dot(v2, v0);
		real d21 = dot(v2, v1);
		real invDenom = 1.0 / (d00 * d11 - d01 * d01);
		real v = (d11 * d20 - d01 * d21) * invDenom;
		real w = (d00 * d21 - d01 * d20) * invDenom;
		real u = 1 - v - w;
		return vec2(u, v);
	}

	inline vec3 interpolate(const triangle &t, const vec2 &p)
	{
		vec3 a = t[0];
		vec3 b = t[1];
		vec3 c = t[2];
		return p[0] * a + p[1] * b + (1 - p[0] - p[1]) * c;
	}

	inline bool inside(const vec2 &b)
	{
		return b[0] >= 0 && b[1] >= 0 && b[0] + b[1] <= 1;
	}

	inline ivec2 operator * (const ivec2 &a, float b)
	{
		return ivec2(sint32(a.x * b), sint32(a.y * b));
	}

	template<class T>
	inline void turnLeft(T &a, T &b, T &c)
	{
		std::swap(a, b); // bac
		std::swap(b, c); // bca
	}

	inline void get(Holder<Image> &img, uint32 x, uint32 y, real &result) { result = img->get1(x, y); }
	inline void get(Holder<Image> &img, uint32 x, uint32 y, vec2 &result) { result = img->get2(x, y); }
	inline void get(Holder<Image> &img, uint32 x, uint32 y, vec3 &result) { result = img->get3(x, y); }
	inline void get(Holder<Image> &img, uint32 x, uint32 y, vec4 &result) { result = img->get4(x, y); }

	inline vec3 mc2c(const dualmc::Vertex &v)
	{
		return vec3(v.x, v.y, v.z) * 2 / verticesPerSide - 1;
	}

	inline string v2s(const vec3 &v)
	{
		return stringizer() + v[0] + " " + v[1] + " " + v[2];
	}

	inline string v2s(const vec2 &v)
	{
		return stringizer() + v[0] + " " + v[1];
	}

	void genDensities()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating densities");
		OPTICK_EVENT("genDensities");
		std::vector<vec3> positions;
		densities.reserve(positions.size());
		for (uint32 z = 0; z < verticesPerSide; z++)
		{
			for (uint32 y = 0; y < verticesPerSide; y++)
			{
				for (uint32 x = 0; x < verticesPerSide; x++)
				{
					vec3 p = vec3(x, y, z) * 2 / verticesPerSide - 1;
					real d = terrainDensity(p);
					CAGE_ASSERT(d.valid());
					densities.push_back(d);
				}
			}
		}
	}

	void genSurface()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating surface");
		OPTICK_EVENT("genSurface");
		dualmc::DualMC<float> mc;
		mc.build((float*)densities.data(), verticesPerSide, verticesPerSide, verticesPerSide, 0, true, false, mcVertices, mcIndices);
		std::vector<real>().swap(densities);
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "vertices count: " + mcVertices.size() + ", quads count: " + mcIndices.size());
		if (mcVertices.size() == 0 || mcIndices.size() == 0)
			CAGE_THROW_ERROR(Exception, "generated empty mesh");
	}

	void genTriangles()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating triangles");
		OPTICK_EVENT("genTriangles");
		CAGE_ASSERT(meshVertices.empty());
		CAGE_ASSERT(meshIndices.empty());
		meshVertices.reserve(mcVertices.size());
		for (const auto &it : mcVertices)
		{
			Vertex v;
			v.position = mc2c(it);
			meshVertices.push_back(v);
		}
		meshIndices.reserve(mcIndices.size() * 6 / 4);
		for (const auto &q : mcIndices)
		{
			const uint32 is[4] = { numeric_cast<uint32>(q.i0), numeric_cast<uint32>(q.i1), numeric_cast<uint32>(q.i2), numeric_cast<uint32>(q.i3) };
#define P(I) meshVertices[is[I]].position
			bool which = distanceSquared(P(0), P(2)) < distanceSquared(P(1), P(3)); // split the quad by shorter diagonal
#undef P
			static const int first[6] = { 0,1,2, 0,2,3 };
			static const int second[6] = { 1,2,3, 1,3,0 };
			for (uint32 i : (which ? first : second))
				meshIndices.push_back(is[i]);
			vec3 n;
			{
				vec3 a = meshVertices[is[0]].position;
				vec3 b = meshVertices[is[1]].position;
				vec3 c = meshVertices[is[2]].position;
				n = normalize(cross(b - a, c - a));
			}
			for (uint32 i : is)
				meshVertices[i].normal += n;
		}
		for (auto &it : meshVertices)
			it.normal = normalize(it.normal);
		std::vector<dualmc::Vertex>().swap(mcVertices);
		std::vector<dualmc::Quad>().swap(mcIndices);
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "vertices count: " + meshVertices.size() + ", triangles count: " + meshIndices.size() / 3);
	}

	void computeScale()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "compute scale");
		OPTICK_EVENT("computeScale");
		real sum = 0;
		uint32 tc = numeric_cast<uint32>(meshIndices.size() / 3);
		for (uint32 t = 0; t < tc; t++)
		{
			for (uint32 e1 = 0; e1 < 3; e1++)
			{
				uint32 e2 = (e1 + 1) % 3;
				vec3 p1 = meshVertices[meshIndices[t * 3 + e1]].position;
				vec3 p2 = meshVertices[meshIndices[t * 3 + e2]].position;
				real d = distance(p1, p2);
				sum += d;
			}
		}
		real scale = tc * 3 / sum;
		for (auto &it : meshVertices)
			it.position *= scale;
		planetScale *= scale;
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "current scale: " + scale + ", total scale: " + planetScale);
	}

	void simplifyMesh()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "simplifying mesh");
		OPTICK_EVENT("simplifyMesh");
		std::vector<ng_mesh::MeshVertex> vs;
		vs.reserve(meshVertices.size());
		for (const auto &it : meshVertices)
		{
			ng_mesh::MeshVertex v;
			v.xyz = vec4(it.position, 1);
			v.normal = vec4(it.normal, 0);
			v.colour = vec4(it.uv, 0, 0);
			vs.push_back(v);
		}

		uint32 tc = numeric_cast<uint32>(meshIndices.size() / 3);
		std::vector<ng_mesh::MeshTriangle> ts;
		ts.reserve(tc);
		for (uint32 ti = 0; ti < tc; ti++)
		{
			ng_mesh::MeshTriangle t;
			t.indices_[0] = meshIndices[ti * 3 + 0];
			t.indices_[1] = meshIndices[ti * 3 + 1];
			t.indices_[2] = meshIndices[ti * 3 + 2];
			ts.push_back(t);
		}

		ng_mesh::MeshBuffer mb;
		mb.vertices = vs.data();
		mb.numVertices = vs.size();
		mb.triangles = ts.data();
		mb.numTriangles = ts.size();

		ng_mesh::MeshSimplificationOptions opts;
		ng_mesh::simplify(&mb, opts);

		CAGE_ASSERT(mb.numVertices <= vs.size());
		CAGE_ASSERT(mb.numTriangles <= ts.size());
		vs.resize(mb.numVertices);
		ts.resize(mb.numTriangles);

		meshVertices.clear();
		for (const auto &it : vs)
		{
			Vertex v;
			v.position = vec3(it.xyz);
			v.normal = normalize(vec3(it.normal));
			v.uv = vec2(it.colour);
			meshVertices.push_back(v);
		}

		meshIndices.clear();
		for (const auto &it : ts)
		{
			meshIndices.push_back(it.indices_[0]);
			meshIndices.push_back(it.indices_[1]);
			meshIndices.push_back(it.indices_[2]);
		}

		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "vertices count: " + meshVertices.size() + ", triangles count: " + meshIndices.size() / 3);
	}

	void genUvs()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating uvs");
		OPTICK_EVENT("genUvs");
		atlas = newAtlas();

		{
			OPTICK_EVENT("AddMesh");
			xatlas::MeshDecl decl;
			decl.indexCount = numeric_cast<uint32>(meshIndices.size());
			decl.indexData = meshIndices.data();
			decl.indexFormat = xatlas::IndexFormat::UInt32;
			decl.vertexCount = numeric_cast<uint32>(meshVertices.size());
			decl.vertexPositionData = &meshVertices[0].position;
			decl.vertexNormalData = &meshVertices[0].normal;
			decl.vertexPositionStride = sizeof(Vertex);
			decl.vertexNormalStride = sizeof(Vertex);
			xatlas::AddMesh(atlas.get(), decl);
		}

		{
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
				pack.texelsPerUnit = texelsPerSegment.value;
				pack.padding = 2;
				pack.bilinear = true;
				pack.blockAlign = true;
				xatlas::PackCharts(atlas.get(), pack);
			}
			CAGE_ASSERT(atlas->meshCount == 1);
			CAGE_ASSERT(atlas->atlasCount == 1);
		}

		{
			OPTICK_EVENT("apply");
			std::vector<Vertex> vs;
			xatlas::Mesh *m = atlas->meshes;
			vs.reserve(m->vertexCount);
			const vec2 whInv = 1 / vec2(atlas->width - 1, atlas->height - 1);
			for (uint32 i = 0; i < m->vertexCount; i++)
			{
				const xatlas::Vertex &a = m->vertexArray[i];
				Vertex v;
				v.position = meshVertices[a.xref].position;
				v.normal = meshVertices[a.xref].normal;
				v.uv = vec2(a.uv[0], a.uv[1]) * whInv;
				vs.push_back(v);
			}
			meshVertices.swap(vs);
			std::vector<uint32> is(m->indexArray, m->indexArray + m->indexCount);
			meshIndices.swap(is);
		}
	}

	void genTextures()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating textures");
		OPTICK_EVENT("genTextures");
		const uint32 w = atlas->width * textureUpscale;
		const uint32 h = atlas->height * textureUpscale;
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "texture resolution: " + w + "x" + h);

		std::vector<triangle> triPos;
		std::vector<triangle> triNorms;
		std::vector<triangle> triUvs;
		{
			OPTICK_EVENT("prepTris");
			const xatlas::Mesh *m = atlas->meshes;
			const uint32 triCount = m->indexCount / 3;
			triPos.reserve(triCount);
			triNorms.reserve(triCount);
			triUvs.reserve(triCount);
			for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
			{
				triangle p;
				triangle n;
				triangle u;
				const uint32 *ids = m->indexArray + triIdx * 3;
				for (uint32 i = 0; i < 3; i++)
				{
					const Vertex &v = meshVertices[ids[i]];
					p[i] = v.position;
					n[i] = v.normal;
					u[i] = vec3(v.uv, 0);
				}
				triPos.push_back(p);
				triNorms.push_back(n);
				triUvs.push_back(u);
			}
		}

		std::vector<vec3> positions;
		std::vector<vec3> normals;
		std::vector<uint32> xs;
		std::vector<uint32> ys;
		{
			OPTICK_EVENT("prepCoords");
			positions.reserve(w * h);
			normals.reserve(w * h);
			xs.reserve(w * h);
			ys.reserve(w * h);
			const vec2 whInv = 1 / vec2(w - 1, h - 1);

			const xatlas::Mesh *m = atlas->meshes;
			const uint32 triCount = m->indexCount / 3;
			for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
			{
				const uint32 *vertIds = m->indexArray + triIdx * 3;
				const float *vertUvs[3] = {
					m->vertexArray[vertIds[0]].uv,
					m->vertexArray[vertIds[1]].uv,
					m->vertexArray[vertIds[2]].uv
				};
				ivec2 t0 = ivec2(sint32(vertUvs[0][0] * textureUpscale), sint32(vertUvs[0][1] * textureUpscale));
				ivec2 t1 = ivec2(sint32(vertUvs[1][0] * textureUpscale), sint32(vertUvs[1][1] * textureUpscale));
				ivec2 t2 = ivec2(sint32(vertUvs[2][0] * textureUpscale), sint32(vertUvs[2][1] * textureUpscale));
				// inspired by https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
				if (t0.y > t1.y)
					std::swap(t0, t1);
				if (t0.y > t2.y)
					std::swap(t0, t2);
				if (t1.y > t2.y)
					std::swap(t1, t2);
				sint32 totalHeight = t2.y - t0.y;
				float totalHeightInv = 1.f / totalHeight;
				for (sint32 i = 0; i < totalHeight; i++)
				{
					bool secondHalf = i > t1.y - t0.y || t1.y == t0.y;
					uint32 segmentHeight = secondHalf ? t2.y - t1.y : t1.y - t0.y;
					float alpha = i * totalHeightInv;
					float beta = (float)(i - (secondHalf ? t1.y - t0.y : 0)) / segmentHeight;
					ivec2 A = t0 + (t2 - t0) * alpha;
					ivec2 B = secondHalf ? t1 + (t2 - t1) * beta : t0 + (t1 - t0) * beta;
					if (A.x > B.x)
						std::swap(A, B);
					for (sint32 x = A.x; x <= B.x; x++)
					{
						sint32 y = t0.y + i;
						vec2 uv = vec2(x, y) * whInv;
						vec2 b = barycoord(triUvs[triIdx], uv);
						CAGE_ASSERT(b.valid());
						positions.push_back(interpolate(triPos[triIdx], b));
						normals.push_back(normalize(interpolate(triNorms[triIdx], b)));
						xs.push_back(x);
						ys.push_back(y);
					}
				}
			}
		}

		albedo = newImage();
		albedo->empty(w, h, 3);
		special = newImage();
		special->empty(w, h, 2);
		heightMap = newImage();
		heightMap->empty(w, h, 1);
		{
			OPTICK_EVENT("pixelColors");
			uint32 *xi = xs.data();
			uint32 *yi = ys.data();
			uint32 cnt = numeric_cast<uint32>(positions.size());
			for (uint32 i = 0; i < cnt; i++)
			{
				vec3 a;
				vec2 s;
				real h;
				terrainMaterial(positions[i], normals[i], a, s, h);
				albedo->set(*xi, *yi, a);
				special->set(*xi, *yi, s);
				heightMap->set(*xi, *yi, h);
				xi++;
				yi++;
			}
		}

		albedo->verticalFlip();
		special->verticalFlip();
		heightMap->verticalFlip();
	}

	template<class T>
	void inpaintProcess(Holder<Image> &src, Holder<Image> &dst)
	{
		uint32 w = src->width();
		uint32 h = src->height();
		for (uint32 y = 0; y < h; y++)
		{
			for (uint32 x = 0; x < w; x++)
			{
				T m;
				get(src, x, y, m);
				if (m == T())
				{
					uint32 cnt = 0;
					uint32 sy = numeric_cast<uint32>(clamp(sint32(y) - 1, 0, sint32(h) - 1));
					uint32 ey = numeric_cast<uint32>(clamp(sint32(y) + 1, 0, sint32(h) - 1));
					uint32 sx = numeric_cast<uint32>(clamp(sint32(x) - 1, 0, sint32(w) - 1));
					uint32 ex = numeric_cast<uint32>(clamp(sint32(x) + 1, 0, sint32(w) - 1));
					T a;
					for (uint32 yy = sy; yy <= ey; yy++)
					{
						for (uint32 xx = sx; xx <= ex; xx++)
						{
							get(src, xx, yy, a);
							if (a != T())
							{
								m += a;
								cnt++;
							}
						}
					}
					if (cnt > 0)
						dst->set(x, y, m / cnt);
				}
				else
					dst->set(x, y, m);
			}
		}
	}

	void inpaintImage(Holder<Image> &img)
	{
		if (!img)
			return;
		OPTICK_EVENT("inpaintImage");
		uint32 c = img->channels();
		Holder<Image> tmp = newImage();
		tmp->empty(img->width(), img->height(), c, img->bytesPerChannel());
		switch (c)
		{
		case 1: inpaintProcess<real>(img, tmp); break;
		case 2: inpaintProcess<vec2>(img, tmp); break;
		case 3: inpaintProcess<vec3>(img, tmp); break;
		case 4: inpaintProcess<vec4>(img, tmp); break;
		}
		std::swap(img, tmp);
	}

	void inpaintEntry(uint32 idx, uint32)
	{
		switch (idx)
		{
		case 0:
			for (uint32 i = 0; i < 7; i++)
				inpaintImage(albedo);
			break;
		case 1:
			for (uint32 i = 0; i < 7; i++)
				inpaintImage(special);
			break;
		case 2:
			for (uint32 i = 0; i < 7; i++)
				inpaintImage(heightMap);
			break;
		}
	}

	void inpaintTextures()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "inpainting textures");
		OPTICK_EVENT("inpaintTextures");
		Holder<ThreadPool> thr = newThreadPool("inpaint_", 3);
		thr->function.bind<&inpaintEntry>();
		thr->run();
	}

	void generatePathProperties()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "generating path properties");
		OPTICK_EVENT("generatePathProperties");
		pathProperties.reserve(meshVertices.size());
		for (const auto &it : meshVertices)
		{
			uint32 type;
			real difficulty;
			terrainPathProperties(it.position, it.normal, type, difficulty);
			difficulty = clamp(difficulty, 0, 1);
			pathProperties.push_back(vec2(difficulty, (type + 0.5) / 8));
		}
	}
}

void generateTerrain()
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating");
	OPTICK_EVENT("generateTerrain");
	genDensities();
	genSurface();
	genTriangles();
	computeScale();
	simplifyMesh();
	computeScale();
	genUvs();
	genTextures();
	inpaintTextures();
	generatePathProperties();
}

void exportTerrain()
{
	CAGE_LOG(SeverityEnum::Info, "generator", "exporting");
	OPTICK_EVENT("exportTerrain");

	Holder<Filesystem> fs = newFilesystem();
	fs->changeDir(stringizer() + "output/" + globalSeed);
	fs->remove("."); // remove previous output

	{ // write unnatural-map
		Holder<File> f = fs->openFile("unnatural-map.ini", FileMode(false, true));
		f->writeLine("[map]");
		f->writeLine("name = Unnatural Planet");
		f->writeLine("version = 0");
		f->writeLine("base = true");
		f->writeLine("[description]");
		f->writeLine(stringizer() + "Seed: " + globalSeed);
		{
			uint32 y, M, d, h, m, s;
			detail::getSystemDateTime(y, M, d, h, m, s);
			f->writeLine(stringizer() + "Date: " + detail::formatDateTime(y, M, d, h, m, s));
		}
		f->writeLine("[authors]");
		f->writeLine("unnatural-planets");
		f->writeLine("[assets]");
		f->writeLine("pack = planet.pack");
		f->writeLine("navigation = planet-navigation.obj");
		f->writeLine("collider = planet-collider.obj");
		f->writeLine("[generator]");
		f->writeLine("name = unnatural-planets");
		f->writeLine("url = https://github.com/ucpu/unnatural-planets.git");
		f->writeLine(stringizer() + "seed = " + globalSeed);
		f->writeLine("feel free to edit this file to your needs");
		f->writeLine("but leave the generator section intact, thanks");
	}

	{ // write scene file
		Holder<File> f = fs->openFile("scene.ini", FileMode(false, true));
		f->writeLine("[]");
		f->writeLine("object = planet.object");
	}

	// go to the data subdirectory
	fs->changeDir("data");

	{ // write textures
		fs->openFile("planet-albedo.png", FileMode(false, true))->writeBuffer(albedo->encodeBuffer());
		fs->openFile("planet-special.png", FileMode(false, true))->writeBuffer(special->encodeBuffer());
		fs->openFile("planet-height.png", FileMode(false, true))->writeBuffer(heightMap->encodeBuffer());
	}

	{ // write geometry for rendering
		Holder<File> f = fs->openFile("planet-render.obj", FileMode(false, true));
		f->writeLine("mtllib planet.mtl");
		f->writeLine("o render");
		f->writeLine("usemtl planet");
		for (const Vertex &v : meshVertices)
			f->writeLine(stringizer() + "v " + v2s(v.position));
		for (const Vertex &v : meshVertices)
			f->writeLine(stringizer() + "vn " + v2s(v.normal));
		for (const Vertex &v : meshVertices)
			f->writeLine(stringizer() + "vt " + v2s(v.uv));
		uint32 cnt = numeric_cast<uint32>(meshIndices.size()) / 3;
		for (uint32 i = 0; i < cnt; i++)
		{
			string s = "f ";
			for (uint32 j = 0; j < 3; j++)
			{
				uint32 k = meshIndices[i * 3 + j] + 1;
				s += stringizer() + k + "/" + k + "/" + k + " ";
			}
			f->writeLine(s);
		}
	}

	{ // write geometry for navigation
		Holder<File> f = fs->openFile("planet-navigation.obj", FileMode(false, true));
		f->writeLine("o navigation");
		for (const Vertex &v : meshVertices)
			f->writeLine(stringizer() + "v " + v2s(v.position));
		for (const Vertex &v : meshVertices)
			f->writeLine(stringizer() + "vn " + v2s(v.normal));
		for (const vec2 &v : pathProperties)
			f->writeLine(stringizer() + "vt " + v2s(v));
		uint32 cnt = numeric_cast<uint32>(meshIndices.size()) / 3;
		for (uint32 i = 0; i < cnt; i++)
		{
			string s = "f ";
			for (uint32 j = 0; j < 3; j++)
			{
				uint32 k = meshIndices[i * 3 + j] + 1;
				s += stringizer() + k + "/" + k + "/" + k + " ";
			}
			f->writeLine(s);
		}
	}

	{ // write geometry for navigation
		Holder<File> f = fs->openFile("planet-collider.obj", FileMode(false, true));
		f->writeLine("o collider");
		for (const Vertex &v : meshVertices)
			f->writeLine(stringizer() + "v " + v2s(v.position));
		uint32 cnt = numeric_cast<uint32>(meshIndices.size()) / 3;
		for (uint32 i = 0; i < cnt; i++)
		{
			string s = "f ";
			for (uint32 j = 0; j < 3; j++)
			{
				uint32 k = meshIndices[i * 3 + j] + 1;
				s += stringizer() + k + " ";
			}
			f->writeLine(s);
		}
	}

	{ // write mtl file with link to albedo texture
		Holder<File> f = fs->openFile("planet.mtl", FileMode(false, true));
		f->writeLine("newmtl planet");
		f->writeLine("map_Kd planet-albedo.png");
		f->writeLine("map_bump planet-height.png");
	}

	{ // write cpm material file
		Holder<File> f = fs->openFile("planet.cpm", FileMode(false, true));
		f->writeLine("[textures]");
		f->writeLine("albedo = planet-albedo.png");
		f->writeLine("special = planet-special.png");
		f->writeLine("normal = planet-height.png");
	}

	{ // object file
		Holder<File> f = fs->openFile("planet.object", FileMode(false, true));
		f->writeLine("[]");
		f->writeLine("planet-render.obj");
	}

	{ // pack file
		Holder<File> f = fs->openFile("planet.pack", FileMode(false, true));
		f->writeLine("[]");
		f->writeLine("planet.object");
	}

	{ // generate asset configuration
		Holder<File> f = fs->openFile("planet.assets", FileMode(false, true));
		f->writeLine("[]");
		f->writeLine("scheme = texture");
		f->writeLine("srgb = true");
		f->writeLine("planet-albedo.png");
		f->writeLine("[]");
		f->writeLine("scheme = texture");
		f->writeLine("planet-special.png");
		f->writeLine("[]");
		f->writeLine("scheme = texture");
		f->writeLine("convert = heightToNormal");
		f->writeLine("planet-height.png");
		f->writeLine("[]");
		f->writeLine("scheme = mesh");
		f->writeLine("material = planet.cpm");
		f->writeLine("tangents = true");
		f->writeLine("planet-render.obj");
		f->writeLine("[]");
		f->writeLine("scheme = mesh");
		f->writeLine("planet-navigation.obj");
		f->writeLine("[]");
		f->writeLine("scheme = collider");
		f->writeLine("planet-collider.obj");
		f->writeLine("[]");
		f->writeLine("scheme = object");
		f->writeLine("planet.object");
		f->writeLine("[]");
		f->writeLine("scheme = pack");
		f->writeLine("planet.pack");
	}

	CAGE_LOG(SeverityEnum::Info, "generator", "exported ok");
}
