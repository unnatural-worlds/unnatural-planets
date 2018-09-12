#include "generator.h"

#include <cage-core/utility/color.h>
#include <cage-core/utility/noise.h>

namespace generator
{
	void meshStruct::texInfo::init(uint32 trisCount, uint32 resolution)
	{
		triRes = resolution;
		trisCols = max(numeric_cast<uint32>(sqrt(trisCount / 2)), 1u);
		trisRows = trisCount / trisCols / 2;
		if (trisCount > trisRows * trisCols * 2)
			trisRows++;
		texWidth = trisCols * triRes;
		texHeight = trisRows * triRes;
		invFullTriRes = real(1) / triRes;
		invTrueTriRes = real(1) / (triRes - 5);
	}

	void meshStruct::unwrap(uint32 resolution)
	{
		tex.init(numeric_cast<uint32>(triangles.size()), resolution);
		uint32 tri = 0;
		vec2 cellFull = tex.triRes / vec2(tex.texWidth, tex.texHeight);
		vec2 cell = (tex.triRes - 5) / vec2(tex.texWidth, tex.texHeight);
		vec2 pix = vec2(1, 1) / vec2(tex.texWidth, tex.texHeight);
		for (triangleStruct &t : triangles)
		{
			uint32 col = (tri / 2) % tex.trisCols;
			uint32 row = (tri / 2) / tex.trisCols;
			vec2 base = vec2(col, row) * cellFull;
			if (tri % 2 == 1)
			{ // upper
				base += vec2(1.5, 3.5) * pix;
				t.uvs[0] = base + vec2(0, cell[1]);
				t.uvs[1] = base + cell;
				t.uvs[2] = base;
			}
			else
			{ // lower
				base += vec2(3.5, 1.5) * pix;
				t.uvs[0] = base + vec2(cell[0], 0);
				t.uvs[1] = base;
				t.uvs[2] = base + cell;
			}
			tri++;
		}
	}

	bool meshStruct::pixPos(uint32 x, uint32 y, vec3 &position, vec3 &normal)
	{
		uint32 col = x / tex.triRes;
		uint32 row = y / tex.triRes;
		uint32 c = x - col * tex.triRes;
		uint32 r = y - row * tex.triRes;
		uint32 tri = (row * tex.trisCols + col) * 2;
		if (r > c)
			tri++;
		if (tri >= triangles.size())
			return false;
		real u, v;
		if (r > c)
		{ // upper triangle
			u = (real(c) - 1.0) / (tex.triRes - 3.5 - 1.5);
			v = (real(r) - (tex.triRes - 2.0)) / (1.5 - (tex.triRes - 3.5));
		}
		else
		{ // lower triangle
			u = (real(c) - (tex.triRes - 2.0)) / (3.5 - (tex.triRes - 1.5));
			v = (real(r) - 1.0) / (tex.triRes - 1.5 - 3.5);
		}
		const triangleStruct &t = triangles[tri];
		position = t.coordinates[0] + u * (t.coordinates[1] - t.coordinates[0]) + v * (t.coordinates[2] - t.coordinates[0]);
		normal = t.normals[0] + u * (t.normals[1] - t.normals[0]) + v * (t.normals[2] - t.normals[0]);
		normal = normalize(normal);
		return true;
	}

	namespace
	{
		real rescale(real v, real a, real b) // (a..b) -> (0..1)
		{
			return (v - a) / (b - a);
		}

		real scaleClamp(real v, real a, real b)
		{
			return rescale(clamp(v, a, b), a, b);
		}
	}

	void basicTone(const vec3 &p, vec3 &albedo)
	{
		real h = noiseClouds(uint32(globalSeed) + 1, p * 0.05, 3);
		real s = noiseClouds(uint32(globalSeed) + 2, p * 1.55, 2);
		real v = noiseClouds(uint32(globalSeed) + 3, p * 0.65, 2);
		vec3 hsv = vec3(h, s, v);
		albedo = convertHsvToRgb(hsv);
	}

	void tiledRocks(const vec3 &p, vec3 &albedo, vec4 &special)
	{
		static const vec3 cranny = vec3(7, 12, 8) / 255;
		static const vec3 dark = vec3(113, 101, 77) / 255;
		static const vec3 light = vec3(208, 194, 165) / 255;
		vec3 p1 = p * 3.5;
		vec4 vv = noiseCell(uint32(globalSeed) + 10, p1, noiseDistanceEnum::EuclideanSquared);
		real v = pow(abs(v[2] - v[1]), 0.1);
		vec3 p2 = p * 5;
		real v4 = noiseClouds(uint32(globalSeed) + 11, p2, 2);
		albedo = interpolate(cranny, interpolate(dark, light, v4), v);
		special = vec4(0.8f - v * 0.6, 0, 0, 1);
	}

	void dottedRocks(const vec3 &p, vec3 &albedo, vec4 &special)
	{
		static const vec3 dots = vec3(182, 183, 187) / 255;
		static const vec3 dark = vec3(64, 64, 62) / 255;
		static const vec3 light = vec3(144, 117, 88) / 255;
		vec3 pw1 = p * 4;
		vec3 pw2 = vec3(
			noiseClouds(uint32(globalSeed) + 55, pw1, 2),
			noiseClouds(uint32(globalSeed) + 56, pw1, 2),
			noiseClouds(uint32(globalSeed) + 57, pw1, 2)
		);
		vec3 pc = p * 2 + pw2 * 3;
		real vl = noiseClouds(uint32(globalSeed) + 51, pc, 2);
		real vh = noiseClouds(uint32(globalSeed) + 52, p * 3000, 1);
		vh = smootherstep(scaleClamp(vh, 0.8, 0.9));
		vl = smootherstep(vl);
		albedo = interpolate(interpolate(dark, light, vl), dots, vh);
		special = vec4(0.4, vh, vh * 0.08, 1);
	}

	void veins(const vec3 &p, vec3 &albedo, vec4 &special, real &power)
	{
		static const vec3 dark = vec3(139, 137, 88) / 255;
		static const vec3 light = vec3(250, 247, 255) / 255;
		vec3 p1 = p * 0.1;
		vec4 vv = noiseCell(uint32(globalSeed) + 20, p1, noiseDistanceEnum::EuclideanSquared);
		real v = pow(1 - abs(vv[1] - vv[0]), 10);
		power = smootherstep(scaleClamp(v, 0.93, 1));
		vec3 p2 = p * 250;
		real v3 = noiseClouds(uint32(globalSeed) + 21, p2, 1);
		albedo = interpolate(dark, light, v3);
		special = vec4(0.7, 0.97, 0.05f + v3 * 0.1, 1);
	}

	void moss(const vec3 &p, const vec3 &n, vec3 &albedo, vec4 &special, real &power)
	{
		static const vec3 dark = vec3(68, 85, 43) / 255;
		static const vec3 light = vec3(119, 155, 21) / 255;
		vec3 pc = p * 100;
		real vc = noiseClouds(uint32(globalSeed) + 61, pc, 1);
		albedo = interpolate(dark, light, vc);
		special = vec4(0.75, 0, 0, 1);
		vec3 pph = p * 13;
		real vph = noiseClouds(uint32(globalSeed) + 62, pph, 2);
		vec3 ppl = p * 1;
		real vpl = noiseClouds(uint32(globalSeed) + 63, ppl, 2);
		power = min(scaleClamp(vpl, 0.6, 0.65), scaleClamp(vph, 0.4, 0.5));
		power *= scaleClamp(dot(normalize(p), n), 0.85, 0.9);
		power = smootherstep(power);
	}

	void texel(const vec3 &p, const vec3 &n, vec3 &albedo, vec4 &special)
	{
		//special = vec4(0.5,0,0,1);
		//albedo = convertHsvToRgb(vec3((p[1] + 100) % 1, 1, 1)) * (cage::sin(rads(p[1] * 50)) * 0.5 + 0.5);
		//return;

		vec3 atr, adr, am, tone, av;
		vec4 str, sdr, sm, sv;
		real pv, pm;
		tiledRocks(p, atr, str);
		dottedRocks(p, adr, sdr);
		basicTone(p, tone);
		veins(p, av, sv, pv);
		moss(p, n, am, sm, pm);

		// add low-frequency color tone
		adr = interpolate(adr, tone, 0.25);
		am = interpolate(am, tone, 0.1);

		// mixture of rocks
		{
			real v = noiseClouds(uint32(globalSeed) + 100, p * 0.7, 4);
			v = smootherstep(scaleClamp(v, 0.5, 0.6));
			albedo = interpolate(atr, adr, v);
			special = interpolate(str, sdr, v);
		}

		// add metal veins
		albedo = interpolate(albedo, av, pv);
		special = interpolate(special, sv, pv);

		// add moss
		albedo = interpolate(albedo, am, pm);
		special = interpolate(special, sm, pm);
	}

	void makeTextureCave(meshStruct &m)
	{
		m.albedo = newPngImage();
		m.special = newPngImage();
		m.albedo->empty(m.tex.texWidth, m.tex.texHeight, 3);
		m.special->empty(m.tex.texWidth, m.tex.texHeight, 4);
		for (uint32 y = 0; y < m.tex.texHeight; y++)
		{
			for (uint32 x = 0; x < m.tex.texWidth; x++)
			{
				vec3 position, normal;
				if (!m.pixPos(x, y, position, normal))
					continue;
				vec3 albedo;
				vec4 special;
				texel(position, normal, albedo, special);
				for (uint32 i = 0; i < 3; i++)
					m.albedo->value(x, y, i, albedo[i].value);
				for (uint32 i = 0; i < 4; i++)
					m.special->value(x, y, i, special[i].value);
			}
		}
		m.albedo->verticalFlip();
		m.special->verticalFlip();
		//m.special.clear();
	}
}
