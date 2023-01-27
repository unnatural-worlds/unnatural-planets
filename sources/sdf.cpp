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
	static constexpr Vec3 scale = Vec3(1, 1, 0.3);
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

	static constexpr Real scale = 0.0007;
	static const Vec3 offset = randomRange3(-100, 100);
	const Vec3 p = pos * scale + offset;
	const Real g = sdGyroid(p, 3.23, 0.2, 1.6);
	const Real v = g * 0.7 / scale;
	const Real d = max(length(pos) - 1500, 0) * 0.5;
	return v + d;
}

Real sdfH2O(const Vec3 &pos_)
{
	static constexpr Real scale = 0.75;
	const Vec3 pos = pos_ * scale;
	const Real h1 = sdfSphere(pos - Vec3(-550, 300, 0), 450);
	const Real h2 = sdfSphere(pos - Vec3(+550, 300, 0), 450);
	const Real o = sdfSphere(pos - Vec3(0, -100, 0), 650);
	return smoothMin(o, min(h1, h2), 100) / scale;
}

Real sdfH3O(const Vec3 &pos_)
{
	static constexpr Real scale = 0.75;
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
	static constexpr Real scale = 0.75;
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

Real sdfGear(const Vec3 &pos)
{
	const auto &rotate = [](const Vec2 &p, const Rads a) -> Vec2 {
		const Real s = sin(a);
		const Real c = cos(a);
		const Real x = c * p[0] - s * p[1];
		const Real y = s * p[0] + c * p[1];
		return Vec2(x, y);
	};

	const auto &sdBox = [](const Vec2 &p, const Vec2 &r) -> Real {
		return length(max(abs(p) - r, 0));
	};

	const Vec2 p = Vec2(pos[0], pos[2]);
	static constexpr uint32 teeths = 9;
	const Rads angle = Rads::Full() / teeths;
	const uint32 sector = numeric_cast<uint32>((atan2(p[0], p[1]) / angle).value + teeths + 0.5) % teeths;
	const Vec2 q = rotate(p, sector * -angle);
	const Real b = sdBox(q - Vec2(700, 0), Vec2(150, 70));
	const Real c = abs(length(p) - 450) - 150;
	const Real h = abs(pos[1]) - 30;
	return smoothMax(min(b, c), h, 50) - 50;
}

Real sdfMandelbulb(const Vec3 &pos_)
{
	if (lengthSquared(pos_) < 1e-3)
		return 0;
	const Vec3 pos = pos_ * 0.0004;
	// taken from https://www.shadertoy.com/view/wstcDN and modified
	static constexpr Real Power = 3.0;
	Vec3 z = pos;
	Real dr = 1.0;
	Real r = 0.0;
	for (uint32 i = 0; i < 20; i++) {
		r = length(z);
		if (r > 4.0)
			break;
		Rads theta = acos(z[2] / r);
		Rads phi = atan2(z[1], z[0]);
		dr = pow(r, Power - 1.0) * Power * dr + 1.0;
		Real zr = pow(r, Power);
		theta = theta * Power;
		phi = phi * Power;
		z = zr * Vec3(sin(theta) * cos(phi), sin(phi) * sin(theta), cos(theta));
		z += pos;
	}
	const Real value = 0.5 * log(r) * r / dr;
	//if (randomChance() < 0.0001)
	//	CAGE_LOG(SeverityEnum::Info, "sdf", Stringizer() + value);
	return (value + 0.1) * 150;
}

Real sdfTwistedHexagonalPrism(const Vec3 &pos)
{
	const Rads angle = Rads(pos[1] * 0.001);
	Vec3 p = Vec3(pos[0], pos[2], 0);
	p = Quat(Rads(), Rads(), angle) * p;
	p = Vec3(p[0], p[1], pos[1]);
	return sdfHexagonalPrism(p);
}
