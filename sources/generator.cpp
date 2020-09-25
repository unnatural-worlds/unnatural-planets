#include <cage-core/threadPool.h>
#include <cage-core/concurrent.h>
#include <cage-core/files.h>
#include <cage-core/config.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/timer.h> // formatDateTime

#include "generator.h"

#include <atomic>

string generateName();

namespace
{
	string findOutputDirectory()
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

		uint32 index = 1;
		while (true)
		{
			string pth = pathJoin(root, stringizer() + index);
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

	void exportConfiguration()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");
		OPTICK_EVENT();

		{ // write unnatural-map
			Holder<File> f = newFile(pathJoin(baseDirectory, "unnatural-map.ini"), FileMode(false, true));
			f->writeLine("[map]");
			f->writeLine(stringizer() + "name = " + generateName());
			f->writeLine("version = 0");
			f->writeLine("[description]");
			f->writeLine(baseShapeName);
			{
				uint32 y, M, d, h, m, s;
				detail::getSystemDateTime(y, M, d, h, m, s);
				f->writeLine(stringizer() + "date: " + detail::formatDateTime(y, M, d, h, m, s));
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
			saveCollider(pathJoin(assetsDirectory, "collider.obj"), mesh);
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
			saveRenderMesh(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + ".obj"), msh);
			Holder<Image> albedo, special, heightMap;
			generateMaterials(msh, resolution, resolution, albedo, special, heightMap);
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
				saveDebugMesh(pathJoin(debugDirectory, "renderMesh.obj"), mesh);
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
			std::vector<TerrainTypeEnum> tileTypes;
			std::vector<BiomeEnum> tileBiomes;
			std::vector<real> tileElevations;
			std::vector<real> tileTemperatures;
			std::vector<real> tilePrecipitations;
			generateTileProperties(navMesh, tileTypes, tileBiomes, tileElevations, tileTemperatures, tilePrecipitations, pathJoin(baseDirectory, "tileStats.log"));
			static_assert(sizeof(TerrainTypeEnum) == sizeof(uint8), "invalid reinterpret cast");
			saveNavigationMesh(pathJoin(assetsDirectory, "navmesh.obj"), navMesh, (std::vector<uint8>&)tileTypes);
			generateDoodads(navMesh, tileTypes, tileBiomes, tileElevations, tileTemperatures, tilePrecipitations, assetPackages, pathJoin(baseDirectory, "doodads.ini"), pathJoin(baseDirectory, "doodadStats.log"));
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

	preseedTerrainFunctions();
	baseMesh = generateBaseMesh(250, 200);
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "initial mesh: vertices: " + baseMesh->verticesCount() + ", triangles: " + (baseMesh->indicesCount() / 3));
	if (saveDebugIntermediates)
		saveDebugMesh(pathJoin(debugDirectory, "baseMesh.obj"), baseMesh);
	
	{
		NavmeshProcessor navigation;
		ColliderProcessor collider;
	}
	{
		RenderProcessor render;
		TilesProcessor tiles;
	}

	exportConfiguration();

	CAGE_LOG(SeverityEnum::Info, "generator", "finished generating");

	{
		const string outPath = findOutputDirectory();
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "output directory: " + outPath);
		pathMove(baseDirectory, outPath);
	}

	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}
