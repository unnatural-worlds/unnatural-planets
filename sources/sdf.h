#ifndef sdf_h_wefdzwdf
#define sdf_h_wefdzwdf

#include <cage-core/signedDistanceFunctions.h>

namespace unnatural
{
	using namespace cage;

	Real sdfAsteroid(const Vec3 &p);
	Real sdfBelt(const Vec3 &p);
	Real sdfBowl(const Vec3 &p);
	Real sdfBox(const Vec3 &pos);
	Real sdfBunny(const Vec3 &p);
	Real sdfCapsule(const Vec3 &pos);
	Real sdfCube(const Vec3 &pos);
	Real sdfDisk(const Vec3 &pos);
	Real sdfDoubleTorus(const Vec3 &p);
	Real sdfFibers(const Vec3 &pos);
	Real sdfGear(const Vec3 &pos);
	Real sdfH2O(const Vec3 &pos);
	Real sdfH3O(const Vec3 &pos);
	Real sdfH4O(const Vec3 &pos);
	Real sdfHemispheres(const Vec3 &p);
	Real sdfHexagon(const Vec3 &pos);
	Real sdfHexagonalPrism(const Vec3 &pos);
	Real sdfInsideCube(const Vec3 &p);
	Real sdfKnot(const Vec3 &pos);
	Real sdfMandelbulb(const Vec3 &pos);
	Real sdfMobiusStrip(const Vec3 &pos);
	Real sdfMonkeyHead(const Vec3 &p);
	Real sdfOctahedron(const Vec3 &pos);
	Real sdfPipe(const Vec3 &p);
	Real sdfSphere(const Vec3 &pos);
	Real sdfSquare(const Vec3 &pos);
	Real sdfTetrahedron(const Vec3 &pos);
	Real sdfTorus(const Vec3 &pos);
	Real sdfTorusCross(const Vec3 &p);
	Real sdfTriangularPrism(const Vec3 &pos);
	Real sdfTube(const Vec3 &pos);
	Real sdfTwistedHexagonalPrism(const Vec3 &pos);
	Real sdfTwistedPlane(const Vec3 &p);
	Real sdfWormhole(const Vec3 &p);
}

#endif
