#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>

#include "sdf.h"

// https://iquilezles.org/www/articles/distfunctions/distfunctions.htm

namespace
{
	real smoothMin(real a, real b, real k)
	{
		// https://www.shadertoy.com/view/3ssGWj
		const real h = saturate((b - a) / k * 0.5 + 0.5);
		return interpolate(b, a, h) - k * h * (1 - h);
	}

	real smoothMax(real a, real b, real k)
	{
		return -smoothMin(-a, -b, k);
	}
}

real sdfPlane(const vec3 &pos, const plane &pln)
{
	CAGE_ASSERT(pln.valid());
	vec3 c = pln.normal * pln.d;
	return dot(pln.normal, pos - c);
}

real sdfHexagon(const vec3 &pos)
{
	return sdfPlane(pos, plane(vec3(), normalize(vec3(1))));
}

real sdfSquare(const vec3 &pos)
{
	return sdfPlane(pos, plane(vec3(), vec3(0, 1, 0)));
}

real sdfSphere(const vec3 &pos, real radius)
{
	return length(pos) - radius;
}

real sdfSphere(const vec3 &pos)
{
	return sdfSphere(pos, 1000);
}

real sdfTorus(const vec3 &pos, real major, real minor)
{
	vec2 q = vec2(length(vec2(pos)) - major, pos[2]);
	return length(q) - minor;
}

real sdfTorus(const vec3 &pos)
{
	return sdfTorus(pos, 750, 250);
}

real sdfCylinder(const vec3 &pos, real height, real radius)
{
	vec2 d = abs(vec2(length(vec2(pos)), pos[2])) - vec2(radius, height * 0.5);
	return min(max(d[0], d[1]), 0) + length(max(d, 0));
}

real sdfTube(const vec3 &pos)
{
	return sdfCylinder(pos, 1800, 300) - 100;
}

real sdfDisk(const vec3 &pos)
{
	return sdfCylinder(pos, 200, 800) - 100;
}

real sdfCapsule(const vec3 &pos, real height, real radius)
{
	vec3 p = pos;
	p[2] += height * 0.5;
	p[2] -= clamp(p[2], 0, height);
	return length(p) - radius;
}

real sdfCapsule(const vec3 &pos)
{
	return sdfCapsule(pos, 1400, 300);
}

real sdfBox(const vec3 &pos, const vec3 &radius)
{
	vec3 p = abs(pos) - radius;
	return length(max(p, 0)) + min(max(p[0], max(p[1], p[2])), 0);
}

real sdfBox(const vec3 &pos)
{
	return sdfBox(pos, vec3(900, 500, 500)) - 100;
}

real sdfCube(const vec3 &pos)
{
	return sdfBox(pos, vec3(900)) - 100;
}

real sdfTetrahedron(const vec3 &pos, real radius)
{
	const vec3 corners[4] = { vec3(1,1,1) * radius, vec3(1,-1,-1) * radius, vec3(-1,1,-1) * radius, vec3(-1,-1,1) * radius };
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
		return -r;
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
		return r;
	}
}

real sdfTetrahedron(const vec3 &pos)
{
	return sdfTetrahedron(pos, 900) - 100;
}

real sdfOctahedron(const vec3 &pos, real radius)
{
	vec3 p = abs(pos);
	real m = p[0] + p[1] + p[2] - radius;
	vec3 q;
	if (3.0 * p[0] < m)
		q = p;
	else if (3.0 * p[1] < m)
		q = vec3(p[1], p[2], p[0]);
	else if (3.0 * p[2] < m)
		q = vec3(p[2], p[0], p[1]);
	else
		return m * 0.57735027;
	real k = clamp(0.5 * (q[2] - q[1] + radius), 0.0, radius);
	return length(vec3(q[0], q[1] - radius + k, q[2] - k));
}

real sdfOctahedron(const vec3 &pos)
{
	return sdfOctahedron(pos, 900) - 100;
}

real sdfKnot(const vec3 &pos, real scale, real k)
{
	constexpr real TAU = real::Pi() * 2;
	vec3 p = pos / scale;
	real r = length(vec2(p));
	rads a = atan2(p[0], p[1]);
	rads oa = k * a;
	a = (a % (0.001 * TAU)) - 0.001 * TAU / 2;
	p[0] = r * cos(a) - 5;
	p[1] = r * sin(a);
	vec2 p2 = cos(oa) * vec2(p[0], p[2]) + sin(oa) * vec2(-p[2], p[0]);
	p[0] = abs(p2[0]) - 1.5;
	p[2] = p2[1];
	return (length(p) - 0.7) * scale;
}

real sdfKnot(const vec3 &pos)
{
	return sdfKnot(pos, 130, 1.5);
}

real sdfMobiusStrip(const vec3 &pos, real radius, real majorAxis, real minorAxis)
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
	return sdRotatedRect(proj + vec2(-radius, 0), vec2(majorAxis, minorAxis), planeRotation / 2);
}

real sdfMobiusStrip(const vec3 &pos)
{
	return sdfMobiusStrip(pos, 700, 300, 20) - 100;
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
	return -(v + d);
}

real sdfH2O(const vec3 &pos)
{
	const real h1 = sdfSphere(pos - vec3(-550, 300, 0), 450);
	const real h2 = sdfSphere(pos - vec3(+550, 300, 0), 450);
	const real o = sdfSphere(pos - vec3(0, -100, 0), 650);
	return smoothMin(o, min(h1, h2), 100);
}

real sdfH3O(const vec3 &pos)
{
	const real hs[] = {
		sdfSphere(pos - quat({}, {}, degs(  0)) * vec3(680, 0, 0), 450),
		sdfSphere(pos - quat({}, {}, degs(120)) * vec3(680, 0, 0), 450),
		sdfSphere(pos - quat({}, {}, degs(240)) * vec3(680, 0, 0), 450),
	};
	const real o = sdfSphere(pos, 650);
	const real h = min(min(hs[0], hs[1]), hs[2]);
	return smoothMin(o, h, 100);
}

real sdfH4O(const vec3 &pos)
{
	const real hs[] = {
		sdfSphere(pos - vec3(-550, +400, 0), 450),
		sdfSphere(pos - vec3(+550, +400, 0), 450),
		sdfSphere(pos - vec3(0, -400, -550), 450),
		sdfSphere(pos - vec3(0, -400, +550), 450),
	};
	const real o = sdfSphere(pos, 650);
	const real h = min(min(hs[0], hs[1]), min(hs[2], hs[3]));
	return smoothMin(o, h, 100);
}

real sdfTriangularPrism(const vec3 &pos, real height, real radius)
{
	const vec3 q = abs(pos);
	return max(q[2] - height * 0.5, max(q[0] * 0.866025 + pos[1] * 0.5, -pos[1]) - radius * 0.5);
}

real sdfTriangularPrism(const vec3 &pos)
{
	return sdfTriangularPrism(pos, 1800, 1000) - 100;
}

real sdfHexagonalPrism(const vec3 &pos, real height, real radius)
{
	constexpr vec3 k = vec3(-0.8660254, 0.5, 0.57735);
	vec3 p = abs(pos);
	p -= vec3(2.0 * min(dot(vec2(k), vec2(p)), 0.0) * vec2(k), 0);
	vec2 d = vec2(length(vec2(p) - vec2(clamp(p[0], -k[2] * radius, k[2] * radius), radius)) * sign(p[1] - radius), p[2] - height * 0.5);
	return min(max(d[0], d[1]), 0.0) + length(max(d, 0.0));
}

real sdfHexagonalPrism(const vec3 &pos)
{
	return sdfHexagonalPrism(pos, 1800, 800) - 100;
}
