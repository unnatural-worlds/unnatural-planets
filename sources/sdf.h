#ifndef sdf_h_wefdzwdf
#define sdf_h_wefdzwdf

#include <cage-core/signedDistanceFunctions.h>

using namespace cage;

Real sdfHexagon(const Vec3 &pos);
Real sdfSquare(const Vec3 &pos);
Real sdfSphere(const Vec3 &pos);
Real sdfCapsule(const Vec3 &pos);
Real sdfTube(const Vec3 &pos);
Real sdfDisk(const Vec3 &pos);
Real sdfBox(const Vec3 &pos);
Real sdfCube(const Vec3 &pos);
Real sdfTetrahedron(const Vec3 &pos);
Real sdfOctahedron(const Vec3 &pos);
Real sdfTriangularPrism(const Vec3 &pos);
Real sdfHexagonalPrism(const Vec3 &pos);
Real sdfTorus(const Vec3 &pos);
Real sdfKnot(const Vec3 &pos);
Real sdfMobiusStrip(const Vec3 &pos);
Real sdfFibers(const Vec3 &pos);
Real sdfH2O(const Vec3 &pos);
Real sdfH3O(const Vec3 &pos);
Real sdfH4O(const Vec3 &pos);
Real sdfGear(const Vec3 &pos);
Real sdfMandelbulb(const Vec3 &pos);
Real sdfTwistedHexagonalPrism(const Vec3 &pos);
Real sdfBunny(const Vec3 &p);
Real sdfMonkeyHead(const Vec3 &p);
Real sdfDoubleTorus(const Vec3 &p);
Real sdfTorusCross(const Vec3 &p);
Real sdfBowl(const Vec3 &p);
Real sdfInsideCube(const Vec3 &p);
Real sdfAsteroid(const Vec3 &p);
Real sdfPipe(const Vec3 &p);
Real sdfTwistedPlane(const Vec3 &p);

#endif
