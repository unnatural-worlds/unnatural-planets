#include "generator.h"
#include "functions.h"

#include <cage-core/geometry.h>
#include <cage-engine/core.h> // ivec2

namespace
{
	vec2 barycoord(const triangle &t, const vec2 &p)
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

	vec3 interpolate(const triangle &t, const vec2 &p)
	{
		vec3 a = t[0];
		vec3 b = t[1];
		vec3 c = t[2];
		return p[0] * a + p[1] * b + (1 - p[0] - p[1]) * c;
	}

	ivec2 operator * (const ivec2 &a, float b)
	{
		return ivec2(sint32(a.x * b), sint32(a.y * b));
	}
}

void generateMaterials(const Holder<UPMesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap)
{
	CAGE_LOG(SeverityEnum::Info, "generator", "generating material textures");
	OPTICK_EVENT();

	const uint32 triCount = renderMesh->indices.size() / 3;

	std::vector<triangle> triPos;
	std::vector<triangle> triNorms;
	std::vector<triangle> triUvs;
	{
		OPTICK_EVENT("prepTris");
		triPos.reserve(triCount);
		triNorms.reserve(triCount);
		triUvs.reserve(triCount);
		for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
		{
			triangle p;
			triangle n;
			triangle u;
			const uint32 *ids = renderMesh->indices.data() + triIdx * 3;
			for (uint32 i = 0; i < 3; i++)
			{
				uint32 j = ids[i];
				p[i] = renderMesh->positions[j];
				n[i] = renderMesh->normals[j];
				u[i] = vec3(renderMesh->uvs[j], 0);
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
		positions.reserve(width * height);
		normals.reserve(width * height);
		xs.reserve(width * height);
		ys.reserve(width * height);
		const vec2 whInv = 1 / vec2(width - 1, height - 1);
		for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
		{
			vec3 invUv = 1.0 / vec3(width - 1, height - 1, 1);
			triangle uvTri = triUvs[triIdx];
			vec3 *vertUvs = uvTri.vertices;
			for (int i = 0; i < 3; i++)
				vertUvs[i] *= invUv;
			ivec2 t0 = ivec2(sint32(vertUvs[0][0].value), sint32(vertUvs[0][1].value));
			ivec2 t1 = ivec2(sint32(vertUvs[1][0].value), sint32(vertUvs[1][1].value));
			ivec2 t2 = ivec2(sint32(vertUvs[2][0].value), sint32(vertUvs[2][1].value));
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
	albedo->empty(width, height, 3);
	special = newImage();
	special->empty(width, height, 2);
	heightMap = newImage();
	heightMap->empty(width, height, 1);
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
			functionMaterial(positions[i], normals[i], a, s, h);
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



