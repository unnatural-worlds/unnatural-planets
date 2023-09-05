#include "sdf.h"
#include "math.h"

#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>

namespace unnatural
{
	Real sdfHexagon(const Vec3 &pos)
	{
		return sdfPlane(pos, Plane(Vec3(), normalize(Vec3(1, 1, -1))));
	}

	Real sdfSquare(const Vec3 &pos)
	{
		return sdfPlane(pos, Plane(Vec3(), Vec3(0, 0, -1)));
	}

	Real sdfSphere(const Vec3 &pos)
	{
		return sdfSphere(pos, 1100);
	}

	Real sdfCapsule(const Vec3 &pos)
	{
		return sdfCapsule(Vec3(pos[0], pos[2], pos[1]), 2500, 600);
	}

	Real sdfTube(const Vec3 &pos)
	{
		return sdfCylinder(Vec3(pos[0], pos[2], pos[1]), 10000, 800);
	}

	Real sdfDisk(const Vec3 &pos)
	{
		return sdfCylinder(pos, 250, 1100) - 200;
	}

	Real sdfBox(const Vec3 &pos)
	{
		return sdfBox(pos, Vec3(550, 1000, 550)) - 200;
	}

	Real sdfCube(const Vec3 &pos)
	{
		return sdfBox(pos, Vec3(650)) - 200;
	}

	Real sdfTetrahedron(const Vec3 &pos)
	{
		return sdfTetrahedron(pos, 1000) - 200;
	}

	Real sdfOctahedron(const Vec3 &pos)
	{
		return sdfOctahedron(pos, 1400) - 200;
	}

	namespace
	{
		Real sdfTriangularPrismImpl(const Vec3 &pos, Real height, Real radius)
		{
			const Triangle t = Triangle(Vec3(0, radius, 0), Vec3(0, radius, 0) * Quat(Degs(), Degs(), Degs(120)), Vec3(0, radius, 0) * Quat(Degs(), Degs(), Degs(-120)));
			Vec3 p = pos;
			p[2] = max(abs(p[2]) - height * 0.5, 0);
			return distance(p, t);
		}
	}

	Real sdfTriangularPrism(const Vec3 &pos)
	{
		return sdfTriangularPrismImpl(Vec3(pos[0], pos[2], pos[1]), 1700, 1000) - 200;
	}

	Real sdfHexagonalPrism(const Vec3 &pos)
	{
		return sdfHexagonalPrism(Vec3(pos[0], pos[2], pos[1]), 900, 650) - 200;
	}

	Real sdfTorus(const Vec3 &pos)
	{
		return sdfTorus(pos, 950, 530);
	}

	Real sdfKnot(const Vec3 &pos)
	{
		static constexpr Vec3 scale = Vec3(1, 1, 0.3);
		return sdfKnot(pos * scale, 1600, 1.5) / length(scale);
	}

	namespace
	{
		Real sdfMobiusStripImpl(const Vec3 &pos, Real radius, Real majorAxis, Real minorAxis)
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
	}

	Real sdfMobiusStrip(const Vec3 &pos)
	{
		return sdfMobiusStripImpl(Vec3(pos[0], pos[2], pos[1]), 1000, 400, 40) - 200;
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
		return smoothMin(o, min(h1, h2), 150) / scale;
	}

	Real sdfH3O(const Vec3 &pos_)
	{
		static constexpr Real scale = 0.75;
		const Vec3 pos = pos_ * scale;
		const Real hs[] = {
			sdfSphere(pos - Quat({}, {}, Degs(0)) * Vec3(680, 0, 0), 450),
			sdfSphere(pos - Quat({}, {}, Degs(120)) * Vec3(680, 0, 0), 450),
			sdfSphere(pos - Quat({}, {}, Degs(240)) * Vec3(680, 0, 0), 450),
		};
		const Real o = sdfSphere(pos, 650);
		const Real h = min(min(hs[0], hs[1]), hs[2]);
		return smoothMin(o, h, 150) / scale;
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
		return smoothMin(o, h, 150) / scale;
	}

	Real sdfGear(const Vec3 &pos_)
	{
		const auto &rotate = [](const Vec2 &p, const Rads a) -> Vec2
		{
			const Real s = sin(a);
			const Real c = cos(a);
			const Real x = c * p[0] - s * p[1];
			const Real y = s * p[0] + c * p[1];
			return Vec2(x, y);
		};

		const auto &sdBox = [](const Vec2 &p, const Vec2 &r) -> Real { return length(max(abs(p) - r, 0)); };

		static constexpr Real scale = 2;
		const Vec3 pos = Vec3(pos_[0], pos_[2], pos_[1]) / scale;
		const Vec2 p = Vec2(pos[0], pos[2]);
		static constexpr uint32 teeths = 9;
		const Rads angle = Rads::Full() / teeths;
		const uint32 sector = numeric_cast<uint32>((atan2(p[0], p[1]) / angle).value + teeths + 0.5) % teeths;
		const Vec2 q = rotate(p, sector * -angle);
		const Real b = sdBox(q - Vec2(700, 0), Vec2(150, 70));
		const Real c = abs(length(p) - 450) - 150;
		const Real h = abs(pos[1]) - 30;
		return (smoothMax(min(b, c), h, 50) - 50) * scale;
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
		for (uint32 i = 0; i < 20; i++)
		{
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
		return (value + 0.1) * 150;
	}

	Real sdfTwistedHexagonalPrism(const Vec3 &pos)
	{
		const Rads angle = Rads(pos[1] * 0.001);
		Vec3 p = Vec3(pos[0], pos[2], 0);
		p = Quat(Rads(), Rads(), angle) * p;
		p = Vec3(p[0], pos[1], p[1]);
		return sdfHexagonalPrism(p);
	}

	namespace
	{
		Vec4 sin(Vec4 a)
		{
			for (uint32 i = 0; i < 4; i++)
				a[i] = sin(Rads(a[i]));
			return a;
		}

		Mat4 sin(Mat4 a)
		{
			for (uint32 i = 0; i < 16; i++)
				a[i] = sin(Rads(a[i]));
			return a;
		}

		Real sdfBunnyImpl(const Vec3 &p)
		{
			// https://www.shadertoy.com/view/wtVyWK
			if (length(p) > 1)
				return length(p) - 0.8;
			Vec4 f00 = sin(p[1] * Vec4(-3.02, 1.95, -3.42, -.60) + p[2] * Vec4(3.08, .85, -2.25, -.24) - p[0] * Vec4(-.29, 1.16, -3.74, 2.89) + Vec4(-.71, 4.50, -3.24, -3.50));
			Vec4 f01 = sin(p[1] * Vec4(-.40, -3.61, 3.23, -.14) + p[2] * Vec4(-.36, 3.64, -3.91, 2.66) - p[0] * Vec4(2.90, -.54, -2.75, 2.71) + Vec4(7.02, -5.41, -1.12, -7.41));
			Vec4 f02 = sin(p[1] * Vec4(-1.77, -1.28, -4.29, -3.20) + p[2] * Vec4(-3.49, -2.81, -.64, 2.79) - p[0] * Vec4(3.15, 2.14, -3.85, 1.83) + Vec4(-2.07, 4.49, 5.33, -2.17));
			Vec4 f03 = sin(p[1] * Vec4(-.49, .68, 3.05, .42) + p[2] * Vec4(-2.87, .78, 3.78, -3.41) - p[0] * Vec4(-2.65, .33, .07, -.64) + Vec4(-3.24, -5.90, 1.14, -4.71));
			Vec4 f10 = sin(Mat4(-.34, .06, -.59, -.76, .10, -.19, -.12, .44, .64, -.02, -.26, .15, -.16, .21, .91, .15) * f00 + Mat4(.01, .54, -.77, .11, .06, -.14, .43, .51, -.18, .08, .39, .20, .33, -.49, -.10, .19) * f01 + Mat4(.27, .22, .43, .53, .18, -.17, .23, -.64, -.14, .02, -.10, .16, -.13, -.06, -.04, -.36) * f02 + Mat4(-.13, .29, -.29, .08, 1.13, .02, -.83, .32, -.32, .04, -.31, -.16, .14, -.03, -.20, .39) * f03 + Vec4(.73, -4.28, -1.56, -1.80)) / 1.0 + f00;
			Vec4 f11 = sin(Mat4(-1.11, .55, -.12, -1.00, .16, .15, -.30, .31, -.01, .01, .31, -.42, -.29, .38, -.04, .71) * f00 + Mat4(.96, -.02, .86, .52, -.14, .60, .44, .43, .02, -.15, -.49, -.05, -.06, -.25, -.03, -.22) * f01 + Mat4(.52, .44, -.05, -.11, -.56, -.10, -.61, -.40, -.04, .55, .32, -.07, -.02, .28, .26, -.49) * f02 + Mat4(.02, -.32, .06, -.17, -.59, .00, -.24, .60, -.06, .13, -.21, -.27, -.12, -.14, .58, -.55) * f03 + Vec4(-2.24, -3.48, -.80, 1.41)) / 1.0 + f01;
			Vec4 f12 = sin(Mat4(.44, -.06, -.79, -.46, .05, -.60, .30, .36, .35, .12, .02, .12, .40, -.26, .63, -.21) * f00 + Mat4(-.48, .43, -.73, -.40, .11, -.01, .71, .05, -.25, .25, -.28, -.20, .32, -.02, -.84, .16) * f01 + Mat4(.39, -.07, .90, .36, -.38, -.27, -1.86, -.39, .48, -.20, -.05, .10, -.00, -.21, .29, .63) * f02 + Mat4(.46, -.32, .06, .09, .72, -.47, .81, .78, .90, .02, -.21, .08, -.16, .22, .32, -.13) * f03 + Vec4(3.38, 1.20, .84, 1.41)) / 1.0 + f02;
			Vec4 f13 = sin(Mat4(-.41, -.24, -.71, -.25, -.24, -.75, -.09, .02, -.27, -.42, .02, .03, -.01, .51, -.12, -1.24) * f00 + Mat4(.64, .31, -1.36, .61, -.34, .11, .14, .79, .22, -.16, -.29, -.70, .02, -.37, .49, .39) * f01 + Mat4(.79, .47, .54, -.47, -1.13, -.35, -1.03, -.22, -.67, -.26, .10, .21, -.07, -.73, -.11, .72) * f02 + Mat4(.43, -.23, .13, .09, 1.38, -.63, 1.57, -.20, .39, -.14, .42, .13, -.57, -.08, -.21, .21) * f03 + Vec4(-.34, -3.28, .43, -.52)) / 1.0 + f03;
			f00 = sin(Mat4(-.72, .23, -.89, .52, .38, .19, -.16, -.88, .26, -.37, .09, .63, .29, -.72, .30, -.95) * f10 + Mat4(-.22, -.51, -.42, -.73, -.32, .00, -1.03, 1.17, -.20, -.03, -.13, -.16, -.41, .09, .36, -.84) * f11 + Mat4(-.21, .01, .33, .47, .05, .20, -.44, -1.04, .13, .12, -.13, .31, .01, -.34, .41, -.34) * f12 + Mat4(-.13, -.06, -.39, -.22, .48, .25, .24, -.97, -.34, .14, .42, -.00, -.44, .05, .09, -.95) * f13 + Vec4(.48, .87, -.87, -2.06)) / 1.4 + f10;
			f01 = sin(Mat4(-.27, .29, -.21, .15, .34, -.23, .85, -.09, -1.15, -.24, -.05, -.25, -.12, -.73, -.17, -.37) * f10 + Mat4(-1.11, .35, -.93, -.06, -.79, -.03, -.46, -.37, .60, -.37, -.14, .45, -.03, -.21, .02, .59) * f11 + Mat4(-.92, -.17, -.58, -.18, .58, .60, .83, -1.04, -.80, -.16, .23, -.11, .08, .16, .76, .61) * f12 + Mat4(.29, .45, .30, .39, -.91, .66, -.35, -.35, .21, .16, -.54, -.63, 1.10, -.38, .20, .15) * f13 + Vec4(-1.72, -.14, 1.92, 2.08)) / 1.4 + f11;
			f02 = sin(Mat4(1.00, .66, 1.30, -.51, .88, .25, -.67, .03, -.68, -.08, -.12, -.14, .46, 1.15, .38, -.10) * f10 + Mat4(.51, -.57, .41, -.09, .68, -.50, -.04, -1.01, .20, .44, -.60, .46, -.09, -.37, -1.30, .04) * f11 + Mat4(.14, .29, -.45, -.06, -.65, .33, -.37, -.95, .71, -.07, 1.00, -.60, -1.68, -.20, -.00, -.70) * f12 + Mat4(-.31, .69, .56, .13, .95, .36, .56, .59, -.63, .52, -.30, .17, 1.23, .72, .95, .75) * f13 + Vec4(-.90, -3.26, -.44, -3.11)) / 1.4 + f12;
			f03 = sin(Mat4(.51, -.98, -.28, .16, -.22, -.17, -1.03, .22, .70, -.15, .12, .43, .78, .67, -.85, -.25) * f10 + Mat4(.81, .60, -.89, .61, -1.03, -.33, .60, -.11, -.06, .01, -.02, -.44, .73, .69, 1.02, .62) * f11 + Mat4(-.10, .52, .80, -.65, .40, -.75, .47, 1.56, .03, .05, .08, .31, -.03, .22, -1.63, .07) * f12 + Mat4(-.18, -.07, -1.22, .48, -.01, .56, .07, .15, .24, .25, -.09, -.54, .23, -.08, .20, .36) * f13 + Vec4(-1.11, -4.28, 1.02, -.23)) / 1.4 + f13;
			return dot(f00, Vec4(.09, .12, -.07, -.03)) + dot(f01, Vec4(-.04, .07, -.08, .05)) + dot(f02, Vec4(-.01, .06, -.02, .07)) + dot(f03, Vec4(-.05, .07, .03, .04)) - 0.16;
		}

		Real sdfMonkeyHeadImpl(const Vec3 &p)
		{
			// https://www.shadertoy.com/view/wtdBzn
			if (length(p) > 1.)
				return length(p) - 0.8;
			Vec4 f0_0 = sin(p[1] * Vec4(-.810, -.479, -1.629, 2.114) + p[2] * Vec4(-1.353, 3.836, .544, -.242) + p[0] * Vec4(-3.646, 3.086, 4.005, 1.374) + Vec4(5.029, -.860, -4.521, -7.561));
			Vec4 f0_1 = sin(p[1] * Vec4(.551, -3.838, -1.237, -3.779) + p[2] * Vec4(-2.141, -1.009, -.282, -.317) + p[0] * Vec4(-2.583, 1.946, -.381, 1.358) + Vec4(8.011, -7.240, 7.978, 6.555));
			Vec4 f0_2 = sin(p[1] * Vec4(.292, .009, .083, -3.117) + p[2] * Vec4(2.295, 1.412, -.507, -4.047) + p[0] * Vec4(-3.354, -1.616, -4.604, -1.501) + Vec4(5.055, .519, 3.222, 3.231));
			Vec4 f0_3 = sin(p[1] * Vec4(.644, 1.538, -1.473, -2.874) + p[2] * Vec4(2.361, -4.293, -1.496, 3.907) + p[0] * Vec4(-.440, 2.221, -2.129, -.953) + Vec4(-2.102, 7.426, -6.489, -2.261));
			Vec4 f1_0 = sin(Mat4(.469, -.455, -.016, .000, .041, -.230, .641, .877, -.771, .400, .439, .275, .240, -.504, .618, .868) * f0_0 + Mat4(.497, -.162, .044, .878, .289, -.741, .552, .162, .043, .296, .299, .988, .103, .027, .242, .600) * f0_1 + Mat4(-.030, -.218, .097, .515, -1.296, .868, -.517, .854, .024, -.069, .573, .398, -.072, -.068, -.196, -.232) * f0_2 + Mat4(-.913, .914, -.583, .229, .201, -1.052, -.087, -.908, -.354, -.067, .446, -.428, .458, .056, -.531, .221) * f0_3 + Vec4(2.389, 3.737, 2.563, 1.536)) / 1.0 + f0_0;
			Vec4 f1_1 = sin(Mat4(-.213, -.215, .364, -.477, -.218, -.111, .141, -.462, .724, -.350, -.379, .120, -.372, .407, -.094, 1.104) * f0_0 + Mat4(-.065, .780, .630, -.243, .039, -.759, -.154, .121, -.828, .536, .678, -.823, .513, .830, -.145, .553) * f0_1 + Mat4(.764, .387, .054, -.431, -.049, .524, .044, .891, -.159, .182, .217, .611, .119, -.355, .228, .030) * f0_2 + Mat4(.526, .020, -.521, -.366, -.465, .357, .082, -.368, .160, .649, -.506, -.260, .154, .482, .222, .393) * f0_3 + Vec4(-.538, .688, -1.270, .721)) / 1.0 + f0_1;
			Vec4 f1_2 = sin(Mat4(-.587, -.235, .031, -.583, .384, .405, .167, .339, .778, -.499, -.311, -.154, -.279, 1.064, -.609, -.058) * f0_0 + Mat4(-.191, .059, .284, .086, -.658, .495, -.015, -.757, 1.143, .597, .475, .298, -.124, -.505, -.401, -.212) * f0_1 + Mat4(.221, .421, -.376, .462, .568, .661, .542, -.524, .471, -.726, -.520, .528, .069, .454, .352, .780) * f0_2 + Mat4(.359, -.025, -.730, -.488, -.865, -.103, -.015, .098, .592, .204, .398, -.472, .072, .396, -.346, -.258) * f0_3 + Vec4(3.503, 3.117, 1.734, -2.248)) / 1.0 + f0_2;
			Vec4 f1_3 = sin(Mat4(-.786, .057, .116, .141, -.233, -.241, .325, -.200, .092, -.531, -.053, .152, -.605, -.351, .413, -.274) * f0_0 + Mat4(.104, -.249, .320, -.261, -.095, .060, .786, -.063, -.070, -.468, .052, -.558, -.557, .009, -.192, .280) * f0_1 + Mat4(-.664, .297, -.124, .113, -.284, .734, .425, -.340, .081, -.133, -.049, -.209, -.310, .368, -.003, -.391) * f0_2 + Mat4(.015, -.185, .427, -.442, .191, .139, -.420, -.262, .254, -.068, .632, .151, .395, -.000, .067, .670) * f0_3 + Vec4(-3.185, 1.715, 3.541, 1.726)) / 1.0 + f0_3;
			Vec4 f2_0 = sin(Mat4(-.876, -.128, .620, -.979, -.052, .201, .492, .232, -.393, -1.020, -.527, -.172, -.143, .127, .526, .223) * f1_0 + Mat4(.643, .102, 1.161, -.555, -.587, -1.253, .246, -.111, .884, -.944, -.620, .048, .468, -.753, -.040, -.534) * f1_1 + Mat4(.257, -.309, .046, .005, -.313, 1.355, -.042, -.088, -.046, -.452, .437, .431, .142, -.044, .253, -.020) * f1_2 + Mat4(-.040, .884, -.167, .816, -.312, -.448, -.929, .809, .502, .072, .851, -1.144, .023, .736, -.080, .056) * f1_3 + Vec4(3.092, -.379, -.619, 3.235)) / 1.4 + f1_0;
			Vec4 f2_1 = sin(Mat4(.324, -.439, -.328, -.771, -.307, -.151, .168, .269, -.978, -1.237, .658, -.216, .229, -.954, -.225, -.464) * f1_0 + Mat4(.514, .160, -.566, -.157, -.830, -.443, -.369, -.660, -1.208, 1.357, .158, .107, .275, -.202, -.144, .006) * f1_1 + Mat4(.757, -.337, -.144, -1.115, .723, .438, -.499, -.044, .046, -.262, .427, -.044, .183, .316, -.389, -.124) * f1_2 + Mat4(-1.063, .047, -.112, .194, .351, -.459, -.209, -.313, -1.771, .433, -.779, .359, .213, .522, -.227, .788) * f1_3 + Vec4(-1.385, -.939, -2.511, -.512)) / 1.4 + f1_1;
			Vec4 f2_2 = sin(Mat4(.562, .796, .458, -.584, -1.463, .710, .360, .128, .025, .451, -.141, .300, .276, .329, -.382, -.444) * f1_0 + Mat4(.377, .319, -.050, .104, .513, -.419, -.201, -.242, .129, .014, -.087, .316, -.012, -.618, -.390, -.426) * f1_1 + Mat4(.299, -.244, .703, -.758, .281, -.135, .835, .354, .241, -.139, .414, .848, -.815, .236, .800, .217) * f1_2 + Mat4(.591, -.381, .696, .199, -.191, -.221, .618, -.531, -.554, .518, -.982, -.010, .194, .181, .784, -1.140) * f1_3 + Vec4(3.316, .154, 1.061, 2.237)) / 1.4 + f1_2;
			Vec4 f2_3 = sin(Mat4(.052, -.589, -1.287, -.338, -.999, .079, .048, .152, -.585, .255, -.721, -.240, -.659, .360, -.016, -.408) * f1_0 + Mat4(.186, .057, -.968, .225, .450, -1.048, -.710, .375, .821, -.730, -.037, .656, .333, .159, .292, .066) * f1_1 + Mat4(.296, .372, -.202, -.562, .510, .041, .037, -.401, -.064, -.203, -.500, -.026, -.587, -.020, .603, -.177) * f1_2 + Mat4(.758, .219, .271, -.131, -.116, -.485, .337, .013, -.380, -.716, -.644, .468, -.297, .552, 1.275, -.132) * f1_3 + Vec4(2.659, .165, -2.731, 2.476)) / 1.4 + f1_3;
			return dot(f2_0, Vec4(.062, -.040, -.074, -.047)) + dot(f2_1, Vec4(-.057, -.039, -.071, -.057)) + dot(f2_2, Vec4(-.025, -.054, .044, .045)) + dot(f2_3, Vec4(.048, .066, -.058, .100)) + 0.102;
		}
	}

	Real sdfBunny(const Vec3 &p)
	{
		static constexpr Real scale = 0.0005;
		return sdfBunnyImpl(Vec3(p[0], p[2], p[1]) * scale) / scale;
	}

	Real sdfMonkeyHead(const Vec3 &p)
	{
		static constexpr Real scale = 0.0005;
		return sdfMonkeyHeadImpl(Vec3(p[0], p[2], p[1]) * scale) / scale;
	}

	Real sdfDoubleTorus(const Vec3 &p)
	{
		static constexpr Real ma = 900;
		static constexpr Real mi = 400;
		const Vec3 p1 = Vec3(p[1], p[2], p[0]);
		const Vec3 p2 = Vec3(p[1], p[0], p[2]);
		return smoothMin(sdfTorus(p1, ma, mi), sdfTorus(p2, ma, mi), 200);
	}

	Real sdfTorusCross(const Vec3 &p)
	{
		static constexpr Real ma = 750;
		static constexpr Real mi = 400;
		const Vec3 p1 = Vec3(p[1] - ma, p[2], p[0]);
		const Vec3 p2 = Vec3(p[1] + ma, p[0], p[2]);
		return smoothMin(sdfTorus(p1, ma, mi), sdfTorus(p2, ma, mi), 200);
	}

	Real sdfBowl(const Vec3 &p)
	{
		return 2000 - distance(p, Vec3(0, 0, -3000));
	}

	Real sdfInsideCube(const Vec3 &p)
	{
		return -sdfCube(p);
	}

	Real sdfAsteroid(const Vec3 &p)
	{
		static const Holder<NoiseFunction> shapeNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Simplex;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 2;
			cfg.gain = 0.3;
			cfg.frequency = randomRange(0.65, 0.75);
			cfg.seed = noiseSeed();
			return newNoiseFunction(cfg);
		}();

		const Real l = length(p);
		if (l < 1)
			return -100;
		const Vec3 d = p / l;
		const Real n = shapeNoise->evaluate(d);
		return l + n * 600 - 1300;
	}

	Real sdfPipe(const Vec3 &p_)
	{
		Vec3 p = p_;
		if (p[0] < 0)
		{
			p[0] *= -1;
			std::swap(p[1], p[2]);
		}
		p[1] = abs(p[1]) - 800;
		std::swap(p[1], p[2]);
		return sdfTorus(p, 800, 400);
	}

	Real sdfTwistedPlane(const Vec3 &p)
	{
		const Real rot = 1 - sqr(1 - min(1, abs(p[2]) / 1000));
		const Rads angle = Degs(sign(p[2]) * rot * 35);
		return p[1] * cos(angle) + p[0] * sin(angle);
	}
}
