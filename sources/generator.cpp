#include <cage-core/threadPool.h>
#include <cage-core/concurrent.h>
#include <cage-core/files.h>
#include <cage-core/config.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/polyhedron.h>

#include "terrain.h"
#include "generator.h"
#include "mesh.h"

#include <atomic>
#include <chrono>
#include <ctime>

namespace
{
	string findOutputDirectory(const string &planetName)
	{
		string root;
		try
		{
			root = pathSearchTowardsRoot("output", PathTypeFlags::Directory);
		}
		catch (const Exception &)
		{
			root = "output";
		}

		{
			const string pth = pathJoin(root, pathReplaceInvalidCharacters(planetName));
			if (pathType(pth) == PathTypeFlags::NotFound)
				return pth;
		}

		uint32 index = 1;
		while (true)
		{
			const string pth = pathJoin(root, stringizer() + index);
			if (pathType(pth) == PathTypeFlags::NotFound)
				return pth;
			index++;
		}
	}

	string findTmpDirectory()
	{
		return pathToAbs(pathJoin("tmp", stringizer() + processId()));
	}

	const string baseDirectory = findTmpDirectory();
	const string assetsDirectory = pathJoin(baseDirectory, "data");
	const string debugDirectory = pathJoin(baseDirectory, "intermediate");
	Holder<Polyhedron> baseMesh;
	Holder<Polyhedron> navMesh;
	std::vector<string> assetPackages;
	uint32 renderChunksCount;
	ConfigString baseShapeName("unnatural-planets/planet/shape");
	ConfigBool saveDebugIntermediates("unnatural-planets/generator/saveIntermediateSteps");

	void exportConfiguration(const string &planetName)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");

		{ // write unnatural-map
			Holder<File> f = newFile(pathJoin(baseDirectory, "unnatural-map.ini"), FileMode(false, true));
			f->writeLine("[map]");
			f->writeLine(stringizer() + "name = " + planetName);
			f->writeLine("version = 0");
			f->writeLine("[description]");
			f->writeLine(baseShapeName);
			{
				const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				char buffer[50];
				std::strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
				f->writeLine(stringizer() + "date: " + buffer);
			}
#ifdef CAGE_DEBUG
			f->writeLine("generated with debug build");
#endif // CAGE_DEBUG
			f->writeLine("[authors]");
			f->writeLine("unnatural-planets procedural generator https://github.com/unnatural-worlds/unnatural-planets");
			f->writeLine("[assets]");
			f->writeLine("pack = planet.pack");
			f->writeLine("navigation = navmesh.obj");
			f->writeLine("collider = collider.obj");
			f->writeLine("[packages]");
			f->writeLine("unnatural/base/base.pack");
			for (const string &s : assetPackages)
				f->writeLine(s);
			f->close();
		}

		{ // write scene file
			Holder<File> f = newFile(pathJoin(baseDirectory, "scene.ini"), FileMode(false, true));
			f->writeLine("[]");
			f->writeLine("object = planet.object");
			f->close();
		}

		{ // object file
			Holder<File> f = newFile(pathJoin(assetsDirectory, "planet.object"), FileMode(false, true));
			f->writeLine("[]");
			for (uint32 i = 0; i < renderChunksCount; i++)
				f->writeLine(stringizer() + "chunk-" + i + ".obj");
			f->close();
		}

		{ // pack file
			Holder<File> f = newFile(pathJoin(assetsDirectory, "planet.pack"), FileMode(false, true));
			f->writeLine("[]");
			f->writeLine("planet.object");
			f->close();
		}

		{ // generate asset configuration
			Holder<File> f = newFile(pathJoin(assetsDirectory, "planet.assets"), FileMode(false, true));
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			f->writeLine("srgb = true");
			for (uint32 i = 0; i < renderChunksCount; i++)
				f->writeLine(stringizer() + "chunk-" + i + "-albedo.png");
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			for (uint32 i = 0; i < renderChunksCount; i++)
				f->writeLine(stringizer() + "chunk-" + i + "-special.png");
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			f->writeLine("convert = heightToNormal");
			for (uint32 i = 0; i < renderChunksCount; i++)
				f->writeLine(stringizer() + "chunk-" + i + "-height.png");
			for (uint32 i = 0; i < renderChunksCount; i++)
			{
				f->writeLine("[]");
				f->writeLine("scheme = mesh");
				f->writeLine("tangents = true");
				f->writeLine("instancesLimit = 1");
				f->writeLine(stringizer() + "material = chunk-" + i + ".cpm");
				f->writeLine(stringizer() + "chunk-" + i + ".obj");
			}
			f->writeLine("[]");
			f->writeLine("scheme = mesh");
			f->writeLine("navmesh.obj");
			f->writeLine("[]");
			f->writeLine("scheme = collider");
			f->writeLine("collider.obj");
			f->writeLine("[]");
			f->writeLine("scheme = object");
			f->writeLine("planet.object");
			f->writeLine("[]");
			f->writeLine("scheme = pack");
			f->writeLine("planet.pack");
			f->close();
		}
	}

	struct NavmeshProcessor
	{
		Holder<Thread> thr;

		void processEntry()
		{
			Holder<Polyhedron> mesh = baseMesh->copy();
			meshSimplifyNavmesh(mesh);
			CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "navmesh tiles: " + mesh->verticesCount());
			if (saveDebugIntermediates)
				meshSaveDebug(pathJoin(debugDirectory, "navMeshBase.obj"), mesh);
			navMesh = templates::move(mesh);
		}

		NavmeshProcessor()
		{
			thr = newThread(Delegate<void()>().bind<NavmeshProcessor, &NavmeshProcessor::processEntry>(this), "navmesh");
		}
	};

	struct ColliderProcessor
	{
		Holder<Thread> thr;

		void processEntry()
		{
			Holder<Polyhedron> mesh = baseMesh->copy();
			meshSimplifyCollider(mesh);
			CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "collider: vertices: " + mesh->verticesCount() + ", triangles: " + (mesh->indicesCount() / 3));
			meshSaveCollider(pathJoin(assetsDirectory, "collider.obj"), mesh);
		}

		ColliderProcessor()
		{
			thr = newThread(Delegate<void()>().bind<ColliderProcessor, &ColliderProcessor::processEntry>(this), "collider");
		}
	};

	struct RenderProcessor
	{
		Holder<Thread> thr;

		std::vector<Holder<Polyhedron>> split;
		std::atomic<uint32> completedChunks{ 0 };

		void processOneChunk(uint32 index)
		{
			const auto &msh = split[index];
			const uint32 resolution = meshUnwrap(msh);
			meshSaveRender(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + ".obj"), msh);
			Holder<Image> albedo, special, heightMap;
			generateTextures(msh, resolution, resolution, albedo, special, heightMap);
			albedo->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-albedo.png"));
			special->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-special.png"));
			heightMap->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-height.png"));
			{
				const uint32 completed = ++completedChunks;
				CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "render chunks: " + (100.0f * completed / split.size()) + " %");
			}
		}

		void poolEntry(uint32 threadIndex, uint32 threadsCount)
		{
			uint32 b, e;
			threadPoolTasksSplit(threadIndex, threadsCount, numeric_cast<uint32>(split.size()), b, e);
			for (uint32 i = b; i < e; i++)
				processOneChunk(i);
		}

		void poolProcess()
		{
			Holder<ThreadPool> thrPool;
			thrPool = newThreadPool("chunks_");
			thrPool->function.bind<RenderProcessor, &RenderProcessor::poolEntry>(this);
			thrPool->run();
		}

		void processEntry()
		{
			Holder<Polyhedron> mesh = baseMesh->copy();
			meshSimplifyRender(mesh);
			if (saveDebugIntermediates)
				meshSaveDebug(pathJoin(debugDirectory, "renderMesh.obj"), mesh);
			split = meshSplit(mesh);
			renderChunksCount = numeric_cast<uint32>(split.size());
			CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "render mesh split into " + renderChunksCount + " chunks");
			poolProcess();
		}

		RenderProcessor()
		{
			thr = newThread(Delegate<void()>().bind<RenderProcessor, &RenderProcessor::processEntry>(this), "render");
		}
	};

	struct TilesProcessor
	{
		Holder<Thread> thr;

		void processEntry()
		{
			std::vector<Tile> tiles;
			generateTileProperties(navMesh, tiles, pathJoin(baseDirectory, "tileStats.log"));
			meshSaveNavigation(pathJoin(assetsDirectory, "navmesh.obj"), navMesh, tiles);
			generateDoodads(navMesh, tiles, assetPackages, pathJoin(baseDirectory, "doodads.ini"), pathJoin(baseDirectory, "doodadStats.log"));
		}

		TilesProcessor()
		{
			thr = newThread(Delegate<void()>().bind<TilesProcessor, &TilesProcessor::processEntry>(this), "tiles");
		}
	};
}

void generateEntry()
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "tmp directory: " + baseDirectory);

	terrainPreseed();
	baseMesh = generateBaseMesh(2500, 200);
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "initial mesh: vertices: " + baseMesh->verticesCount() + ", triangles: " + (baseMesh->indicesCount() / 3));
	if (saveDebugIntermediates)
		meshSaveDebug(pathJoin(debugDirectory, "baseMesh.obj"), baseMesh);

	{
		NavmeshProcessor navigation;
		ColliderProcessor collider;
	}
	{
		RenderProcessor render;
		TilesProcessor tiles;
	}

	const string planetName = generateName();

	exportConfiguration(planetName);

	CAGE_LOG(SeverityEnum::Info, "generator", "finished generating");

	{
		const string outPath = findOutputDirectory(planetName);
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "output directory: " + outPath);
		pathMove(baseDirectory, outPath);
	}

	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}
