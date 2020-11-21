#ifndef sdf_h_wefdzwdf
#define sdf_h_wefdzwdf

#include "generator.h"

real sdfPlane(const vec3 &pos, const plane &pln);
real sdfPlane(const vec3 &pos);
real sdfSphere(const vec3 &pos, real radius);
real sdfSphere(const vec3 &pos);
real sdfTorus(const vec3 &pos, real major, real minor);
real sdfTorus(const vec3 &pos);
real sdfCylinder(const vec3 &pos, real r, real h, real rounding);
real sdfCylinder(const vec3 &pos);
real sdfBox(const vec3 &pos, const vec3 &size, real rounding);
real sdfBox(const vec3 &pos);
real sdfTetrahedron(const vec3 &pos, real size, real rounding);
real sdfTetrahedron(const vec3 &pos);
real sdfOctahedron(const vec3 &pos, real size, real rounding);
real sdfOctahedron(const vec3 &pos);
real sdfKnot(const vec3 &pos, real scale, real k);
real sdfKnot(const vec3 &pos);
real sdfPretzel(const vec3 &pos);
real sdfMobiusStrip(const vec3 &pos, real radius, real majorAxis, real minorAxis, real rounding);
real sdfMobiusStrip(const vec3 &pos);
real sdfMolecule(const vec3 &pos);
real sdfH2O(const vec3 &pos);
real sdfH3O(const vec3 &pos);
real sdfH4O(const vec3 &pos);

#endif
