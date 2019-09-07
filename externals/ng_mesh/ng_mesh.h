#ifndef HAS_SIMD_MESH_SIMPLIFY_H_BEEN_INCUDED
#define HAS_SIMD_MESH_SIMPLIFY_H_BEEN_INCUDED

// taken from https://github.com/nickgildea/ng_mesh
// modified

namespace ng_mesh
{
	using cage::vec4;

	struct MeshSimplificationOptions
	{
		// Each iteration involves selecting a fraction of the edges at random as possible 
		// candidates for collapsing. There is likely a sweet spot here trading off against number 
		// of edges processed vs number of invalid collapses generated due to collisions 
		// (the more edges that are processed the higher the chance of collisions happening)
		float edgeFraction = 0.125f;

		// Stop simplifying after a given number of iterations
		int maxIterations = 200;

		// And/or stop simplifying when we've reached a percentage of the input triangles
		float targetPercentage = 0.5f;

		// The maximum allowed error when collapsing an edge (error is calculated as 1.0/qef_error)
		float maxError = 1.f;
	};

	struct MeshVertex
	{
		vec4 xyz, normal, colour;
	};

	struct MeshTriangle
	{
		int indices_[3];
	};

	class MeshBuffer
	{
	public:
		MeshVertex *vertices = nullptr;
		int numVertices = 0;

		MeshTriangle *triangles = nullptr;
		int numTriangles = 0;
	};

	// The MeshBuffer instance will be edited in place
	void simplify(
		MeshBuffer *mesh,
		const MeshSimplificationOptions &options);
}

#endif // HAS_SIMD_MESH_SIMPLIFY_H_BEEN_INCUDED

