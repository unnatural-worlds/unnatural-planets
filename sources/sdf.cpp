#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>

#include "sdf.h"

real sdfPlane(const vec3 &pos, const plane &pln)
{
	CAGE_ASSERT(pln.valid());
	vec3 c = pln.normal * pln.d;
	return -dot(pln.normal, pos - c);
}

real sdfPlane(const vec3 &pos)
{
	return sdfPlane(pos, plane(vec3(), normalize(vec3(1))));
}

real sdfSphere(const vec3 &pos, real radius)
{
	return radius - length(pos);
}

real sdfSphere(const vec3 &pos)
{
	return sdfSphere(pos, 1000);
}

real sdfTorus(const vec3 &pos, real major, real minor)
{
	vec3 c = normalize(pos * vec3(1, 0, 1)) * major;
	if (!valid(c))
		return real::Infinity();
	return minor - distance(pos, c);
}

real sdfTorus(const vec3 &pos)
{
	return sdfTorus(pos, 750, 250);
}

real sdfCylinder(const vec3 &pos, real r, real h, real rounding)
{
	vec2 d = abs(vec2(length(vec2(pos[0], pos[2])), pos[1])) - vec2(r, h);
	return -min(max(d[0], d[1]), 0) - length(max(d, 0)) + rounding;
}

real sdfCylinder(const vec3 &pos)
{
	return sdfCylinder(pos, 800, 700, 50);
}

real sdfBox(const vec3 &pos, const vec3 &size, real rounding)
{
	vec3 p = abs(pos) - size;
	real box = length(max(p, 0)) + min(max(p[0], max(p[1], p[2])), 0) - rounding;
	return -box;
}

real sdfBox(const vec3 &pos)
{
	return sdfBox(pos, vec3(1000, 500, 500), 50);
}

real sdfTetrahedron(const vec3 &pos, real size, real rounding)
{
	const vec3 corners[4] = { vec3(1,1,1)*size, vec3(1,-1,-1)*size, vec3(-1,1,-1)*size, vec3(-1,-1,1)*size };
	const triangle tris[4] = {
		triangle(corners[0], corners[1], corners[2]),
		triangle(corners[0], corners[3], corners[1]),
		triangle(corners[0], corners[2], corners[3]),
		triangle(corners[1], corners[3], corners[2])
	};
	const auto &sideOfAPlane = [](const triangle &tri, const vec3 &pt) -> bool
	{
		plane pl(tri);
		return dot(pl.normal, pt) < -pl.d;
	};
	const bool insides[4] = {
		sideOfAPlane(tris[0], pos),
		sideOfAPlane(tris[1], pos),
		sideOfAPlane(tris[2], pos),
		sideOfAPlane(tris[3], pos)
	};
	const real lens[4] = {
		distance(tris[0], pos),
		distance(tris[1], pos),
		distance(tris[2], pos),
		distance(tris[3], pos)
	};
	if (insides[0] && insides[1] && insides[2] && insides[3])
	{
		real r = min(min(lens[0], lens[1]), min(lens[2], lens[3]));
		CAGE_ASSERT(r.valid() && r.finite());
		return r + rounding;
	}
	else
	{
		real r = real::Infinity();
		for (int i = 0; i < 4; i++)
		{
			if (!insides[i])
				r = min(r, lens[i]);
		}
		CAGE_ASSERT(r.valid() && r.finite());
		return -r + rounding;
	}
}

real sdfTetrahedron(const vec3 &pos)
{
	return sdfTetrahedron(pos, 1000, 50);
}

real sdfOctahedron(const vec3 &pos, real size, real rounding)
{
	vec3 p = abs(pos);
	real m = p[0] + p[1] + p[2] - size;
	vec3 q;
	if (3.0 * p[0] < m)
		q = p;
	else if (3.0 * p[1] < m)
		q = vec3(p[1], p[2], p[0]);
	else if (3.0 * p[2] < m)
		q = vec3(p[2], p[0], p[1]);
	else
		return m * -0.57735027 + rounding;
	real k = clamp(0.5 * (q[2] - q[1] + size), 0.0, size);
	return -length(vec3(q[0], q[1] - size + k, q[2] - k)) + rounding;
}

real sdfOctahedron(const vec3 &pos)
{
	return sdfOctahedron(pos, 1000, 50);
}

real sdfKnot(const vec3 &pos, real scale, real k)
{
	constexpr real TAU = real::Pi() * 2;
	vec3 p = pos / scale;
	real r = length(vec2(p[0], p[1]));
	rads a = atan2(p[0], p[1]);
	rads oa = k * a;
	a = (a % (0.001 * TAU)) - 0.001 * TAU / 2;
	p[0] = r * cos(a) - 5;
	p[1] = r * sin(a);
	vec2 p2 = cos(oa) * vec2(p[0], p[2]) + sin(oa) * vec2(-p[2], p[0]);
	p[0] = abs(p2[0]) - 1.5;
	p[2] = p2[1];
	return (length(p) - 0.7) * -scale;
}

real sdfKnot(const vec3 &pos)
{
	return sdfKnot(pos, 130, 1.5);
}

real sdfPretzel(const vec3 &pos)
{
	vec3 c = normalize(pos * vec3(1, 0, 1));
	rads yaw = atan2(c[0], c[2]);
	rads pitch = atan2(pos[1], 700 - length(vec2(pos[0], pos[2])));
	rads ang = yaw + pitch;
	real t = length(vec2(sin(ang) * 250, cos(ang) * 50));
	real l = distance(pos, c * 700);
	return t - l;
}

real sdfMobiusStrip(const vec3 &pos, real radius, real majorAxis, real minorAxis, real rounding)
{
	const auto &sdAlignedRect = [](const vec2 &point, const vec2 &halfSizes) -> real
	{
		vec2 d = abs(point) - halfSizes;
		return length(max(d, 0)) + min(max(d[0], d[1]), 0);
	};

	const auto &sdRotatedRect = [&](const vec2 &point, const vec2 &halfSizes, rads rotation) -> real
	{
		// rotate the point instead of the rectangle
		rads a = atan2(point[0], point[1]);
		a += rotation;
		vec2 p = vec2(cos(a), sin(a)) * length(point);
		return sdAlignedRect(p, halfSizes);
	};

	rads planeRotation = atan2(pos[0], pos[2]);
	vec2 proj = vec2(dot(vec2(pos[0], pos[2]), normalize(vec2(pos[0], pos[2]))), pos[1]);
	return -sdRotatedRect(proj + vec2(-radius, 0), vec2(majorAxis, minorAxis), planeRotation / 2) + rounding;
}

real sdfMobiusStrip(const vec3 &pos)
{
	return sdfMobiusStrip(pos, 700, 300, 20, 50);
}

real sdfMolecule(const vec3 &pos)
{
	const auto &sdGyroid = [](vec3 p, real scale, real thickness, real bias)
	{
		p *= scale;
		vec3 a = vec3(sin(rads(p[0])), sin(rads(p[1])), sin(rads(p[2])));
		vec3 b = vec3(cos(rads(p[2])), cos(rads(p[0])), cos(rads(p[1])));
		return abs(dot(a, b) + bias) / scale - thickness;
	};

	constexpr real scale = 0.002;
	static const vec3 offset = randomRange3(-100, 100);
	const vec3 p = pos * scale + offset;
	const real g1 = sdGyroid(p, 3.23, 0.03, 1.4);
	const real g2 = sdGyroid(p, 7.78, 0.05, 0.3);
	const real g3 = sdGyroid(p, 12.21, 0.02, 0.1);
	const real g = g1 - g2 * 0.27 - g3 * 0.11;
	const real v = -g * 0.7 / scale;
	const real d = -max(length(pos) - 900, 0) * 0.5;
	return v + d;
}
