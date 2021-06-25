#ifndef sdf_h_wefdzwdf
#define sdf_h_wefdzwdf

#include <cage-core/signedDistanceFunctions.h>

using namespace cage;

real sdfHexagon(const vec3 &pos);
real sdfSquare(const vec3 &pos);
real sdfSphere(const vec3 &pos);
real sdfCapsule(const vec3 &pos);
real sdfTube(const vec3 &pos);
real sdfDisk(const vec3 &pos);
real sdfBox(const vec3 &pos);
real sdfCube(const vec3 &pos);
real sdfTetrahedron(const vec3 &pos);
real sdfOctahedron(const vec3 &pos);
real sdfTriangularPrism(const vec3 &pos);
real sdfHexagonalPrism(const vec3 &pos);
real sdfTorus(const vec3 &pos);
real sdfKnot(const vec3 &pos);
real sdfMobiusStrip(const vec3 &pos);
real sdfFibers(const vec3 &pos);
real sdfH2O(const vec3 &pos);
real sdfH3O(const vec3 &pos);
real sdfH4O(const vec3 &pos);

#endif
