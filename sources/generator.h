#include <vector>
#include <map>
#include <set>

#include <cage-core/core.h>
#include <cage-core/log.h>
#include <cage-core/math.h>
#include <cage-core/geometry.h>
#include <cage-core/utility/png.h>
#include <cage-core/utility/spatial.h>

using namespace cage;

namespace generator
{
	extern uint64 globalSeed;
	extern string assetsName;
	extern string packName;

	struct triangleStruct
	{
		triangle coordinates;
		vec3 normals[3];
		vec2 uvs[3];

		triangleStruct();
		triangleStruct(const vec3 &a, const vec3 &b, const vec3 &c);
	};

	struct meshStruct
	{
		string name;
		std::vector<triangleStruct> triangles;
		holder<pngImageClass> albedo;
		holder<pngImageClass> special;
		string material;
		bool collider;

		struct texInfo
		{
			uint32 triRes;
			uint32 trisCols;
			uint32 trisRows;
			uint32 texWidth;
			uint32 texHeight;
			real invFullTriRes;
			real invTrueTriRes;

			void init(uint32 trisCount, uint32 resolution);
		} tex;

		meshStruct();
		void clear();
		void insideOut();
		aabb box() const;
		void flatShading();
		void smoothShading();
		void subdivide();
		void marching(uint32 side, real *densities, const aabb &box);
		void unwrap(uint32 resolution);
		bool pixPos(uint32 x, uint32 y, vec3 &position, vec3 &normal);
		void save();
	};

	struct graphStruct
	{
		struct nodeStruct
		{
			vec3 position;
			vec3 color;
			real radius;
			//real cost;
		};

		struct linkStruct
		{
			uint32 a, b;
			real radius;
			real cost;

			linkStruct();
			linkStruct(uint32 a, uint32 b);
		};

		struct comparator
		{
			graphStruct *graph;
			comparator(graphStruct *graph);
			bool operator () (const linkStruct &a, const linkStruct &b) const;
		};

		std::vector<nodeStruct> nodes;
		std::set<linkStruct, comparator> links;
		bool oriented;

		graphStruct();
		void clear();
		void addRandomNodes(uint64 seed, uint32 nodesCount);
		void addRandomLinks(uint64 seed, uint32 linksCount, real maxLength, uint32 maxAttempts);
		void generateLinksFull();
		void linkCostsByLength(bool inverse = false);
		void removeNode(uint32 index);
		void removeNodeOverlaps();
		void removeLinkOverlaps();
		void minimumSpanningTree();
		void clamp(const aabb &box);
		void colorizeNodes();
		uint32 componentsCount() const;
	};

	void makeMeshCave(const graphStruct &graph, uint32 side, const aabb &box, meshStruct &mesh);
	void makeMeshMinimap(const graphStruct &graph, meshStruct &mesh);
	void makeTextureCave(meshStruct &mesh);
	void generateMap();
}
