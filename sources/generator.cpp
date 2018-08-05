#include "generator.h"

#include <cage-core/concurrent.h>
#include <cage-core/filesystem.h>
#include <cage-core/utility/noise.h>
#include <cage-core/utility/color.h>
#include <cage-core/utility/threadPool.h>
#include <atomic>

namespace generator
{
	uint64 globalSeed;
	string assetsName;
	string packName;

	namespace
	{
		graphStruct graph;
		std::vector<aabb> boxes;
		std::atomic<uint32> indexGlobal;
		std::atomic<uint32> totalNonemptyParts;
		std::atomic<uint32> totalTriangles;
		std::atomic<uint64> totalTexels;

		void deletePreviousVersion()
		{
			holder<filesystemClass> fs = newFilesystem();
			if (fs->exists(globalSeed))
			{
				CAGE_LOG(severityEnum::Info, "generator", string() + "deleting previous version");
				fs->remove(globalSeed);
				threadSleep(1000 * 50);
			}
		}

		void generateAssetConfiguration()
		{
			assetsName = string() + globalSeed + "/map.asset";
			packName = string() + globalSeed + "/map.pack";

			// add to assets
			{
				holder<fileClass> f = newFile(assetsName, fileMode(false, true, true, true));
				f->writeLine("[]");
				f->writeLine("scheme = pack");
				f->writeLine("map.pack");
			}

			// prepare the asset pack
			{
				holder<fileClass> f = newFile(packName, fileMode(false, true, true, true));
				f->writeLine("[]");
			}
		}

		void generateTheGraph()
		{
			CAGE_LOG(severityEnum::Info, "generator", "generating the graph");
			graph.addRandomNodes(42, 30);
			for (graphStruct::nodeStruct &n : graph.nodes)
				n.radius = (pow(noiseClouds(80, n.position * 3, 3), 2) * 0.2 + 0.2);
			graph.removeNodeOverlaps();
			graph.generateLinksFull();
			graph.linkCostsByLength();
			graph.minimumSpanningTree();
			graph.addRandomLinks(42, 30, 0.7, 100);
			for (const graphStruct::linkStruct &ll : graph.links)
			{
				graphStruct::linkStruct &l = const_cast<graphStruct::linkStruct&>(ll);
				l.radius = (pow(noiseClouds(81, (graph.nodes[l.a].position + graph.nodes[l.b].position) * 0.5 * 3, 3), 3) * 0.2 + 0.15);
			}
			graph.removeLinkOverlaps();
			graph.colorizeNodes();
			CAGE_LOG(severityEnum::Info, "generator", string() + graph.nodes.size() + " nodes, " + graph.links.size() + " links");
		}

		void generateTheBoxes()
		{
			CAGE_LOG(severityEnum::Info, "generator", "partitioning");
			real scale = 25;
			for (graphStruct::nodeStruct &n : graph.nodes)
			{
				n.position *= scale;
				n.radius *= scale;
			}
			for (const graphStruct::linkStruct &ll : graph.links)
			{
				graphStruct::linkStruct &l = const_cast<graphStruct::linkStruct&>(ll);
				l.radius *= scale;
			}
			uint32 side = 9;
			aabb box = aabb(vec3(-1.2, -1.2, -1.2) * scale, vec3(1.2, 1.2, 1.2) * scale);
			vec3 bs = box.size() / side;
			for (uint32 z = 0; z < side; z++)
			{
				for (uint32 y = 0; y < side; y++)
				{
					for (uint32 x = 0; x < side; x++)
					{
						vec3 a = vec3(x, y, z) * bs + box.a;
						boxes.push_back(aabb(a, a + bs));
					}
				}
			}
		}

		void generateMinimap()
		{
			CAGE_LOG(severityEnum::Info, "generator", "generating minimap");
			meshStruct mesh;
			makeMeshMinimap(graph, mesh);
			mesh.name = "minimap";
			mesh.material = "[base]\nemissive = 1\n[flags]\nno-lighting\nno-shadow-cast\n";
			mesh.save();
		}

		void thrEntry(uint32 thrIndex, uint32 thrCnt)
		{
			while (true)
			{
				uint32 index = indexGlobal++;
				if (index >= boxes.size())
					break;
				CAGE_LOG(severityEnum::Info, "generator", string() + "processing part " + index);
				aabb box = boxes[index];
				graphStruct g = graph;
				g.clamp(box);
				CAGE_LOG(severityEnum::Info, "generator", string() + g.nodes.size() + " nodes, " + g.links.size() + " links");
				if (g.nodes.size() == 0 && g.links.size() == 0)
					continue;
				meshStruct m;
				m.name = string() + "cave_" + index;
				makeMeshCave(g, 34, box, m);
				CAGE_LOG(severityEnum::Info, "generator", string() + m.triangles.size() + " triangles");
				if (m.triangles.size() == 0)
					continue;
				m.unwrap(14);
				makeTextureCave(m);
				m.material = "[flags]\ntwo-sided\n";
				m.collider = true;
				m.save();
				totalNonemptyParts++;
				totalTriangles += numeric_cast<uint32>(m.triangles.size());
				totalTexels += m.tex.texWidth * m.tex.texHeight;
			}
		}
	}

	void generateMap()
	{
		indexGlobal = 0;
		totalNonemptyParts = 0;
		totalTriangles = 0;
		totalTexels = 0;

		deletePreviousVersion();
		generateAssetConfiguration();
		generateTheGraph();
		generateTheBoxes();
		generateMinimap();

		// generate the meshes
		{
			holder<threadPoolClass> thrPool = newThreadPool();
			thrPool->function.bind<&thrEntry>();
			thrPool->run();
			CAGE_LOG(severityEnum::Info, "generator", string() + "total non-empty parts: " + (uint32)totalNonemptyParts);
			CAGE_LOG(severityEnum::Info, "generator", string() + "total triangles: " + (uint32)totalTriangles);
			CAGE_LOG(severityEnum::Info, "generator", string() + "total texels: " + (uint64)totalTexels);
		}
	}
}
