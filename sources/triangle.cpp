#include "generator.h"

namespace generator
{
	triangleStruct::triangleStruct()
	{}

	triangleStruct::triangleStruct(const vec3 &a, const vec3 &b, const vec3 &c)
	{
		coordinates[0] = a;
		coordinates[1] = b;
		coordinates[2] = c;
	}
}
