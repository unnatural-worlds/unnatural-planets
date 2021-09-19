#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>
#include <cage-core/geometry.h>

#include "sdf.h"
#include "math.h"

Real sdfHexagon(const Vec3 &pos)
{
	return sdfPlane(pos, Plane(Vec3(), normalize(Vec3(1))));
}

Real sdfSquare(const Vec3 &pos)
{
	return sdfPlane(pos, Plane(Vec3(), Vec3(0, 1, 0)));
}

Real sdfSphere(const Vec3 &pos)
{
	return sdfSphere(pos, 1100);
}

Real sdfCapsule(const Vec3 &pos)
{
	return sdfCapsule(pos, 2500, 600);
}

Real sdfTube(const Vec3 &pos)
{
	return sdfCylinder(pos, 10000, 500) - 100;
}

Real sdfDisk(const Vec3 &pos)
{
	return sdfCylinder(pos, 250, 1100) - 100;
}

Real sdfBox(const Vec3 &pos)
{
	return sdfBox(pos, Vec3(1000, 550, 550)) - 100;
}

Real sdfCube(const Vec3 &pos)
{
	return sdfBox(pos, Vec3(650)) - 100;
}

Real sdfTetrahedron(const Vec3 &pos)
{
	return sdfTetrahedron(pos, 1000) - 100;
}

Real sdfOctahedron(const Vec3 &pos)
{
	return sdfOctahedron(pos, 1400) - 100;
}

Real sdfTriangularPrism(const Vec3 &pos, Real height, Real radius)
{
	const Triangle t = Triangle(Vec3(0, radius, 0), Vec3(0, radius, 0) * Quat(Degs(), Degs(), Degs(120)), Vec3(0, radius, 0) * Quat(Degs(), Degs(), Degs(-120)));
	Vec3 p = pos;
	p[2] = max(abs(p[2]) - height * 0.5, 0);
	return distance(p, t);
}

Real sdfTriangularPrism(const Vec3 &pos)
{
	return sdfTriangularPrism(pos, 1700, 1000) - 100;
}

Real sdfHexagonalPrism(const Vec3 &pos)
{
	return sdfHexagonalPrism(pos, 900, 650) - 100;
}

Real sdfTorus(const Vec3 &pos)
{
	return sdfTorus(pos, 900, 530);
}

Real sdfKnot(const Vec3 &pos)
{
	constexpr Vec3 scale = Vec3(1, 1, 0.3);
	return sdfKnot(pos * scale, 1600, 1.5) / length(scale);
}

Real sdfMobiusStrip(const Vec3 &pos, Real radius, Real majorAxis, Real minorAxis)
{
	const auto &sdAlignedRect = [](const Vec2 &point, const Vec2 &halfSizes) -> Real
	{
		Vec2 d = abs(point) - halfSizes;
		return length(max(d, 0)) + min(max(d[0], d[1]), 0);
	};

	const auto &sdRotatedRect = [&](const Vec2 &point, const Vec2 &halfSizes, Rads rotation) -> Real
	{
		// rotate the point instead of the rectangle
		Rads a = atan2(point[0], point[1]);
		a += rotation;
		Vec2 p = Vec2(cos(a), sin(a)) * length(point);
		return sdAlignedRect(p, halfSizes);
	};

	const Rads planeRotation = atan2(pos[0], pos[2]);
	const Vec2 proj = Vec2(dot(Vec2(pos[0], pos[2]), normalize(Vec2(pos[0], pos[2]))), pos[1]);
	return sdRotatedRect(proj + Vec2(-radius, 0), Vec2(majorAxis, minorAxis), planeRotation / 2);
}

Real sdfMobiusStrip(const Vec3 &pos)
{
	return sdfMobiusStrip(pos, 1000, 400, 40) - 100;
}

Real sdfFibers(const Vec3 &pos)
{
	const auto &sdGyroid = [](Vec3 p, Real scale, Real thickness, Real bias)
	{
		p *= scale;
		const Vec3 a = Vec3(sin(Rads(p[0])), sin(Rads(p[1])), sin(Rads(p[2])));
		const Vec3 b = Vec3(cos(Rads(p[2])), cos(Rads(p[0])), cos(Rads(p[1])));
		return abs(dot(a, b) + bias) / scale - thickness;
	};

	constexpr Real scale = 0.0007;
	static const Vec3 offset = randomRange3(-100, 100);
	const Vec3 p = pos * scale + offset;
	const Real g = sdGyroid(p, 3.23, 0.2, 1.6);
	const Real v = g * 0.7 / scale;
	const Real d = max(length(pos) - 1500, 0) * 0.5;
	return v + d;
}

Real sdfH2O(const Vec3 &pos_)
{
	constexpr Real scale = 0.75;
	const Vec3 pos = pos_ * scale;
	const Real h1 = sdfSphere(pos - Vec3(-550, 300, 0), 450);
	const Real h2 = sdfSphere(pos - Vec3(+550, 300, 0), 450);
	const Real o = sdfSphere(pos - Vec3(0, -100, 0), 650);
	return smoothMin(o, min(h1, h2), 100) / scale;
}

Real sdfH3O(const Vec3 &pos_)
{
	constexpr Real scale = 0.75;
	const Vec3 pos = pos_ * scale;
	const Real hs[] = {
		sdfSphere(pos - Quat({}, {}, Degs(  0)) * Vec3(680, 0, 0), 450),
		sdfSphere(pos - Quat({}, {}, Degs(120)) * Vec3(680, 0, 0), 450),
		sdfSphere(pos - Quat({}, {}, Degs(240)) * Vec3(680, 0, 0), 450),
	};
	const Real o = sdfSphere(pos, 650);
	const Real h = min(min(hs[0], hs[1]), hs[2]);
	return smoothMin(o, h, 100) / scale;
}

Real sdfH4O(const Vec3 &pos_)
{
	constexpr Real scale = 0.75;
	const Vec3 pos = pos_ * scale;
	const Real hs[] = {
		sdfSphere(pos - Vec3(-550, +400, 0), 450),
		sdfSphere(pos - Vec3(+550, +400, 0), 450),
		sdfSphere(pos - Vec3(0, -400, -550), 450),
		sdfSphere(pos - Vec3(0, -400, +550), 450),
	};
	const Real o = sdfSphere(pos, 650);
	const Real h = min(min(hs[0], hs[1]), min(hs[2], hs[3]));
	return smoothMin(o, h, 100) / scale;
}
