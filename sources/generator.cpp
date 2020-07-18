#include "generator.h"

#include <cage-core/threadPool.h>
#include <cage-core/concurrent.h>
#include <cage-core/files.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/timer.h> // formatDateTime

#include <atomic>

namespace
{
	string findBaseDirectory()
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

	const string baseDirectory = findBaseDirectory();
	const string assetsDirectory = pathJoin(baseDirectory, "data");
	Holder<const Polyhedron> baseMesh;

	void exportConfiguration(uint32 renderChunksCount)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");
		OPTICK_EVENT();

		{ // write unnatural-map
			Holder<File> f = newFile(pathJoin(baseDirectory, "unnatural-map.ini"), FileMode(false, true));
			f->writeLine("[map]");
			f->writeLine("name = Unnatural Planet");
			f->writeLine("version = 0");
			f->writeLine("base = true");
			f->writeLine("[description]");
			f->writeLine(stringizer() + "Seed1: " + detail::getApplicationRandomGenerator().s[0]);
			f->writeLine(stringizer() + "Seed2: " + detail::getApplicationRandomGenerator().s[1]);
			{
				uint32 y, M, d, h, m, s;
				detail::getSystemDateTime(y, M, d, h, m, s);
				f->writeLine(stringizer() + "Date: " + detail::formatDateTime(y, M, d, h, m, s));
			}
			f->writeLine("[authors]");
			f->writeLine("unnatural-planets procedural generator");
			f->writeLine("[assets]");
			f->writeLine("pack = planet.pack");
			f->writeLine("navigation = navmesh.obj");
			f->writeLine("collider = collider.obj");
		}

		{ // write scene file
			Holder<File> f = newFile(pathJoin(baseDirectory, "scene.ini"), FileMode(false, true));
			f->writeLine("[]");
			f->writeLine("object = planet.object");
		}

		{ // object file
			Holder<File> f = newFile(pathJoin(assetsDirectory, "planet.object"), FileMode(false, true));
			f->writeLine("[]");
			for (uint32 i = 0; i < renderChunksCount; i++)
				f->writeLine(stringizer() + "chunk-" + i + ".obj");
		}

		{ // pack file
			Holder<File> f = newFile(pathJoin(assetsDirectory, "planet.pack"), FileMode(false, true));
			f->writeLine("[]");
			f->writeLine("planet.object");
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
			std::vector<uint8> terrainTypes = generateTileProperties(mesh, pathJoin(baseDirectory, "stats.log"));
			saveNavigationMesh(pathJoin(assetsDirectory, "navmesh.obj"), mesh, terrainTypes);
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

		SplitResult split;
		std::atomic<uint32> completedChunks{ 0 };

		void processOneChunk(uint32 index)
		{
			const auto &msh = split.meshes[index];
			const uint32 resolution = meshUnwrap(msh);
			saveRenderMesh(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + ".obj"), msh);
			Holder<Image> albedo, special, heightMap;
			generateMaterials(msh, resolution, resolution, albedo, special, heightMap);
			albedo->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-albedo.png"));
			special->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-special.png"));
			heightMap->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-height.png"));
			{
				uint32 completed = ++completedChunks;
				CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "render chunks: " + (100.0 * completed / split.meshes.size()) + " %");
			}
		}

		void poolEntry(uint32 threadIndex, uint32 threadsCount)
		{
			uint32 b, e;
			threadPoolTasksSplit(threadIndex, threadsCount, numeric_cast<uint32>(split.meshes.size()), b, e);
			for (uint32 i = b; i < e; i++)
				processOneChunk(i);
		}

		void poolProcess()
		{
			Holder<ThreadPool> thrPool;
			thrPool = newThreadPool("process_");
			thrPool->function.bind<RenderProcessor, &RenderProcessor::poolEntry>(this);
			thrPool->run();
		}

		void processEntry()
		{
			Holder<Polyhedron> mesh = baseMesh->copy();
			meshSimplifyRender(mesh);
			split = meshSplit(mesh);
			CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "render mesh separated into " + split.meshes.size() + " chunks");
			exportConfiguration(numeric_cast<uint32>(split.meshes.size()));
			poolProcess();
		}

		RenderProcessor()
		{
			thr = newThread(Delegate<void()>().bind<RenderProcessor, &RenderProcessor::processEntry>(this), "render");
		}
	};
}

void generateEntry()
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "target directory: " + pathToAbs(baseDirectory));
	baseMesh = generateBaseMesh(250, 200).cast<const Polyhedron>();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "initial mesh: vertices: " + baseMesh->verticesCount() + ", triangles: " + (baseMesh->indicesCount() / 3));
	
	{
		NavmeshProcessor nav;
		ColliderProcessor col;
		RenderProcessor ren;
	}

	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}

