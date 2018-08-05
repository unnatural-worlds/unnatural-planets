#include <algorithm>

#include "generator.h"

namespace generator
{
	namespace
	{
		vec3 mix(vec3 valueMax, vec3 valueMin, real dataMax, real dataMin, bool inverted = false)
		{
			if (inverted)
			{
				std::swap(valueMax, valueMin);
				std::swap(dataMax, dataMin);
			}
			CAGE_ASSERT_RUNTIME(dataMax > dataMin);
			CAGE_ASSERT_RUNTIME(0 <= dataMax && 0 >= dataMin);
			real f = (0 - dataMin) / (dataMax - dataMin);
			return interpolate(valueMin, valueMax, f);
		}

		void oneAbove(std::vector<vec3> &positions, std::vector<vec3> &normals, vec3 corners[4], vec3 grads[4], real data[4], uint32 which)
		{
			for (uint32 i = 0; i < 4; i++)
			{
				if (i != which)
				{
					positions.push_back(mix(corners[which], corners[i], data[which], data[i]));
					normals.push_back(normalize(mix(grads[which], grads[i], data[which], data[i])));
				}
			}
		}

		void threeAbove(std::vector<vec3> &positions, std::vector<vec3> &normals, vec3 corners[4], vec3 grads[4], real data[4], uint32 which)
		{
			for (uint32 i = 0; i < 4; i++)
			{
				if (i != which)
				{
					positions.push_back(mix(corners[which], corners[i], data[which], data[i], true));
					normals.push_back(normalize(mix(grads[which], grads[i], data[which], data[i], true)));
				}
			}
		}

		void twoAbove(std::vector<vec3> &positions, std::vector<vec3> &normals, vec3 corners[4], vec3 grads[4], real data[4], uint32 above)
		{
			vec3 c[4]; // coordinates
			vec3 g[4]; // gradients
			uint32 k = 0;
			for (uint32 i = 0; i < 4; i++)
			{
				for (uint32 j = i + 1; j < 4; j++)
				{
					if ((data[i] <= 0) == (data[j] <= 0))
						continue;

					CAGE_ASSERT_RUNTIME(k < 4);
					bool inv = data[i] <= data[j];
					c[k] = mix(corners[i], corners[j], data[i], data[j], inv);
					g[k] = normalize(mix(grads[i], grads[j], data[i], data[j], inv));
					k++;
				}
			}
			CAGE_ASSERT_RUNTIME(k == 4);

			uint32 indices[] = { 0, 1, 2, 3 };
			uint32 bestPerm[4];

			real shortestCircumference = real::PositiveInfinity;
			real currentCircumference;
			do {
				currentCircumference = 0;
				for (uint32 i = 1; i < 4; i++)
				{
					currentCircumference += distance(c[indices[i]], c[indices[i - 1]]);
				}
				currentCircumference += distance(c[indices[3]], c[indices[0]]);

				if (currentCircumference < shortestCircumference)
				{
					shortestCircumference = currentCircumference;
					detail::memcpy(bestPerm, indices, sizeof(int) * 4);
				}
			} while (std::next_permutation(indices, indices + 4));

			positions.push_back(c[bestPerm[0]]); normals.push_back(g[bestPerm[0]]);
			positions.push_back(c[bestPerm[1]]); normals.push_back(g[bestPerm[1]]);
			positions.push_back(c[bestPerm[3]]); normals.push_back(g[bestPerm[3]]);

			positions.push_back(c[bestPerm[1]]); normals.push_back(g[bestPerm[1]]);
			positions.push_back(c[bestPerm[2]]); normals.push_back(g[bestPerm[2]]);
			positions.push_back(c[bestPerm[3]]); normals.push_back(g[bestPerm[3]]);
		}

		void solveTetrahedron(std::vector<vec3> &positions, std::vector<vec3> &normals, vec3 corners[4], vec3 grads[4], real data[4])
		{
			uint32 above = 0;
			for (uint32 i = 0; i < 4; i++)
				above += (data[i] > 0) << i;

			switch (above)
			{
				// no isosurface
			case 0:  return;
			case 15: return;
				// one corner above
			case 1: return oneAbove(positions, normals, corners, grads, data, 0);
			case 2: return oneAbove(positions, normals, corners, grads, data, 1);
			case 4: return oneAbove(positions, normals, corners, grads, data, 2);
			case 8: return oneAbove(positions, normals, corners, grads, data, 3);
				// two corners above
			default: return twoAbove(positions, normals, corners, grads, data, above);
				// three corners above
			case 7:  return threeAbove(positions, normals, corners, grads, data, 3);
			case 11: return threeAbove(positions, normals, corners, grads, data, 2);
			case 13: return threeAbove(positions, normals, corners, grads, data, 1);
			case 14: return threeAbove(positions, normals, corners, grads, data, 0);
			}
		}

		void selectTetrahedron(const vec3 cin[8], const vec3 gin[8], const real din[8], vec3 cout[4], vec3 gout[4], real dout[4], uint32 a, uint32 b, uint32 c, uint32 d)
		{
			cout[0] = cin[a];
			gout[0] = gin[a];
			dout[0] = din[a];
			cout[1] = cin[b];
			gout[1] = gin[b];
			dout[1] = din[b];
			cout[2] = cin[c];
			gout[2] = gin[c];
			dout[2] = din[c];
			cout[3] = cin[d];
			gout[3] = gin[d];
			dout[3] = din[d];
		}

		void solveCube(std::vector<vec3> &positions, std::vector<vec3> &normals, const vec3 corners[8], const vec3 gradients[8], const real data[8])
		{
			uint32 above = 0;
			uint32 below = 0;
			for (uint32 i = 0; i < 8; i++)
			{
				if (data[i] > 0)
					above++;
				else
					below++;
			}
			if (above == 0 || below == 0)
				return; // early exit

			vec3 c[4];
			vec3 g[4];
			real d[4];
			selectTetrahedron(corners, gradients, data, c, g, d, 0, 1, 2, 6); solveTetrahedron(positions, normals, c, g, d);
			selectTetrahedron(corners, gradients, data, c, g, d, 0, 1, 4, 6); solveTetrahedron(positions, normals, c, g, d);
			selectTetrahedron(corners, gradients, data, c, g, d, 1, 2, 3, 6); solveTetrahedron(positions, normals, c, g, d);
			selectTetrahedron(corners, gradients, data, c, g, d, 1, 3, 6, 7); solveTetrahedron(positions, normals, c, g, d);
			selectTetrahedron(corners, gradients, data, c, g, d, 1, 4, 5, 6); solveTetrahedron(positions, normals, c, g, d);
			selectTetrahedron(corners, gradients, data, c, g, d, 1, 5, 6, 7); solveTetrahedron(positions, normals, c, g, d);
		}

		real evalData(uint32 side1, real *densities, sint32 x, sint32 y, sint32 z)
		{
			if (x < 0 || x >= numeric_cast<sint32>(side1)) return 0;
			if (y < 0 || y >= numeric_cast<sint32>(side1)) return 0;
			if (z < 0 || z >= numeric_cast<sint32>(side1)) return 0;
			return densities[((z * side1) + y) * side1 + x];
		}

		vec3 evalGrad(uint32 side1, real *densities, sint32 x, sint32 y, sint32 z)
		{
			return vec3(
				evalData(side1, densities, x + 1, y, z) - evalData(side1, densities, x - 1, y, z),
				evalData(side1, densities, x, y + 1, z) - evalData(side1, densities, x, y - 1, z),
				evalData(side1, densities, x, y, z + 1) - evalData(side1, densities, x, y, z - 1)
			);
		}
	}

	void meshStruct::marching(uint32 side, real *densities, const aabb &box)
	{
		clear();
		uint32 side1 = side + 1;
		vec3 bs = box.size() / side;
		std::vector<vec3> positions;
		std::vector<vec3> normals;
		positions.reserve(side * side * side * 10);
		normals.reserve(side * side * side * 10);
		for (uint32 z = 0; z < side; z++)
		{
			for (uint32 y = 0; y < side; y++)
			{
				for (uint32 x = 0; x < side; x++)
				{
					vec3 p = vec3(x, y, z) * bs + box.a;
					aabb b = aabb(p, p + bs);
					vec3 corners[8];
					vec3 gradients[8];
					real data[8];
					for (uint32 zz = 0; zz < 2; zz++)
					{
						for (uint32 yy = 0; yy < 2; yy++)
						{
							for (uint32 xx = 0; xx < 2; xx++)
							{
								uint32 idx = ((zz * 2) + yy) * 2 + xx;
								corners[idx] = vec3(
									xx == 0 ? b.a[0] : b.b[0],
									yy == 0 ? b.a[1] : b.b[1],
									zz == 0 ? b.a[2] : b.b[2]
								);
								gradients[idx] = evalGrad(side1, densities, x + xx, y + yy, z + zz);
								data[idx] = evalData(side1, densities, x + xx, y + yy, z + zz);
							}
						}
					}
					solveCube(positions, normals, corners, gradients, data);
				}
			}
		}
		CAGE_ASSERT_RUNTIME(positions.size() % 3 == 0);
		uint32 s = numeric_cast<uint32>(positions.size()) / 3;
		for (uint32 i = 0; i < s; i++)
		{
			triangleStruct t(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
			if (t.coordinates.area() < 1e-6)
				continue; // degenerate triangle
			for (uint32 j = 0; j < 3; j++)
				t.normals[j] = normals[i * 3 + j];

			// fix triangle winding
			{
				vec3 n = normalize(t.normals[0] + t.normals[1] + t.normals[2]);
				vec3 c = t.coordinates.normal();
				if (dot(n, c) < 0)
				{
					std::swap(t.coordinates[1], t.coordinates[2]);
					std::swap(t.normals[1], t.normals[2]);
				}
			}

			triangles.push_back(t);
		}
	}
}
