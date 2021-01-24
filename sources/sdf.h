#ifndef sdf_h_wefdzwdf
#define sdf_h_wefdzwdf

#include <cage-core/math.h>

using namespace cage;

real sdfPlane(const vec3 &pos, const plane &pln);
real sdfHexagon(const vec3 &pos);
real sdfSquare(const vec3 &pos);
real sdfSphere(const vec3 &pos, real radius);
real sdfSphere(const vec3 &pos);
real sdfTorus(const vec3 &pos, real major, real minor);
real sdfTorus(const vec3 &pos);
real sdfCylinder(const vec3 &pos, real height, real radius);
real sdfTube(const vec3 &pos);
real sdfDisk(const vec3 &pos);
real sdfCapsule(const vec3 &pos, real height, real radius);
real sdfCapsule(const vec3 &pos);
real sdfBox(const vec3 &pos, const vec3 &radius);
real sdfBox(const vec3 &pos);
real sdfCube(const vec3 &pos);
real sdfTetrahedron(const vec3 &pos, real radius);
real sdfTetrahedron(const vec3 &pos);
real sdfOctahedron(const vec3 &pos, real radius);
real sdfOctahedron(const vec3 &pos);
real sdfKnot(const vec3 &pos, real scale, real k);
real sdfKnot(const vec3 &pos);
real sdfMobiusStrip(const vec3 &pos, real radius, real majorAxis, real minorAxis);
real sdfMobiusStrip(const vec3 &pos);
real sdfMolecule(const vec3 &pos);
real sdfH2O(const vec3 &pos);
real sdfH3O(const vec3 &pos);
real sdfH4O(const vec3 &pos);
real sdfTriangularPrism(const vec3 &pos, real height, real radius);
real sdfTriangularPrism(const vec3 &pos);
real sdfHexagonalPrism(const vec3 &pos, real height, real radius);
real sdfHexagonalPrism(const vec3 &pos);

#endif
