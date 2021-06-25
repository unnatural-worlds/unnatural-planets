#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>

#include "sdf.h"
#include "math.h"

real sdfHexagon(const vec3 &pos)
{
	return sdfPlane(pos, Plane(vec3(), normalize(vec3(1))));
}

real sdfSquare(const vec3 &pos)
{
	return sdfPlane(pos, Plane(vec3(), vec3(0, 1, 0)));
}

real sdfSphere(const vec3 &pos)
{
	return sdfSphere(pos, 1000);
}

real sdfCapsule(const vec3 &pos)
{
	return sdfCapsule(pos, 1400, 300);
}

real sdfTube(const vec3 &pos)
{
	return sdfCylinder(pos, 1800, 300) - 100;
}

real sdfDisk(const vec3 &pos)
{
	return sdfCylinder(pos, 200, 800) - 100;
}

real sdfBox(const vec3 &pos)
{
	return sdfBox(pos, vec3(900, 500, 500)) - 100;
}

real sdfCube(const vec3 &pos)
{
	return sdfBox(pos, vec3(900)) - 100;
}

real sdfTetrahedron(const vec3 &pos)
{
	return sdfTetrahedron(pos, 900) - 100;
}

real sdfOctahedron(const vec3 &pos)
{
	return sdfOctahedron(pos, 900) - 100;
}

real sdfTriangularPrism(const vec3 &pos, real height, real radius)
{
	const Triangle t = Triangle(vec3(0, radius, 0), vec3(0, radius, 0) * quat(degs(), degs(), degs(120)), vec3(0, radius, 0) * quat(degs(), degs(), degs(-120)));
	vec3 p = pos;
	p[2] = max(abs(p[2]) - height * 0.5, 0);
	return distance(p, t);
}

real sdfTriangularPrism(const vec3 &pos)
{
	return sdfTriangularPrism(pos, 1800, 1000) - 100;
}

real sdfHexagonalPrism(const vec3 &pos)
{
	return sdfHexagonalPrism(pos, 900, 800) - 100;
}

real sdfTorus(const vec3 &pos)
{
	return sdfTorus(pos, 750, 250);
}

real sdfKnot(const vec3 &pos)
{
	return sdfKnot(pos, 1000, 1.5);
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

real sdfFibers(const vec3 &pos)
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
