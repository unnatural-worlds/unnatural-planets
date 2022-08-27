#include <cage-core/concurrent.h>
#include <cage-core/tasks.h>
#include <cage-core/files.h>
#include <cage-core/config.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/mesh.h>
#include <cage-core/process.h>
#include <cage-core/debug.h>
#include <cage-core/string.h>

#include "terrain.h"
#include "generator.h"
#include "mesh.h"

#include <atomic>
#include <chrono>
#include <ctime>

namespace
{
	String findOutputDirectory(const String &planetName)
	{
		String root;
		try
		{
			root = pathSearchTowardsRoot("output", PathTypeFlags::Directory);
		}
		catch (const Exception &)
		{
			root = "output";
		}

		{
			String name = pathReplaceInvalidCharacters(planetName);
			name = replace(name, " ", "_");
#ifdef CAGE_DEBUG
			name += "_debug";
#endif // CAGE_DEBUG
			const String pth = pathJoin(root, name);
			if (pathType(pth) == PathTypeFlags::NotFound)
				return pth;
		}

		uint32 index = 1;
		while (true)
		{
			const String pth = pathJoin(root, Stringizer() + index);
			if (pathType(pth) == PathTypeFlags::NotFound)
				return pth;
			index++;
		}
	}

	String findTmpDirectory()
	{
		return pathToAbs(pathJoin("tmp", Stringizer() + currentProcessId()));
	}

	const String planetName = generateName();
	const String baseDirectory = findTmpDirectory();
	const String assetsDirectory = pathJoin(baseDirectory, "data");
	const String debugDirectory = pathJoin(baseDirectory, "intermediate");
	ConfigString configShapeMode("unnatural-planets/shape/mode");
	ConfigBool configDebugSaveIntermediate("unnatural-planets/debug/saveIntermediate");
	ConfigBool configPreviewEnable("unnatural-planets/preview/enable");
	std::vector<String> assetPackages;
	struct Chunk
	{
		String mesh;
		String albedo, pbr, normal;
		bool transparency = false;
	};
	std::vector<Chunk> chunks;
	Holder<Mutex> chunksMutex = newMutex();

	void exportConfiguration()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");

		{ // write unnatural-map
			Holder<File> f = writeFile(pathJoin(baseDirectory, "unnatural-map.ini"));
			f->writeLine("[map]");
			f->writeLine(Stringizer() + "name = " + planetName);
			f->writeLine("version = 0");
			f->writeLine("[description]");
			f->writeLine(configShapeMode);
			{
				const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				char buffer[50];
				std::strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
				f->writeLine(Stringizer() + "date: " + buffer);
			}
#ifdef CAGE_DEBUG
			f->writeLine("generated with DEBUG build");
#endif // CAGE_DEBUG
			f->writeLine("[authors]");
			f->writeLine("unnatural-planets procedural generator https://github.com/unnatural-worlds/unnatural-planets");
			f->writeLine("[assets]");
			f->writeLine("pack = planet.pack");
			f->writeLine("navigation = navmesh.glb");
			f->writeLine("collider = collider.glb");
			f->writeLine("[packages]");
			f->writeLine("unnatural/base/base.pack");
			for (const String &s : assetPackages)
				f->writeLine(s);
			f->close();
		}

		{ // write scene file
			Holder<File> f = writeFile(pathJoin(baseDirectory, "scene.ini"));
			f->writeLine("[]");
			f->writeLine("object = planet.object");
			f->close();
		}

		{ // object file
			Holder<File> f = writeFile(pathJoin(assetsDirectory, "planet.object"));
			f->writeLine("[]");
			for (const Chunk &c : chunks)
				f->writeLine(c.mesh);
			f->close();
		}

		{ // pack file
			Holder<File> f = writeFile(pathJoin(assetsDirectory, "planet.pack"));
			f->writeLine("[]");
			f->writeLine("planet.object");
			f->close();
		}

		{ // generate asset configuration
			Holder<File> f = writeFile(pathJoin(assetsDirectory, "planet.assets"));
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			f->writeLine("srgb = true");
			for (const Chunk &c : chunks)
				if (!c.albedo.empty())
					f->writeLine(c.albedo);
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			f->writeLine("convert = gltfToSpecial");
			for (const Chunk &c : chunks)
				if (!c.pbr.empty())
					f->writeLine(c.pbr);
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			f->writeLine("normal = true");
			for (const Chunk &c : chunks)
				if (!c.normal.empty())
					f->writeLine(c.normal);
			f->writeLine("[]");
			f->writeLine("scheme = model");
			f->writeLine("tangents = true");
			for (const Chunk &c : chunks)
				f->writeLine(c.mesh);
			f->writeLine("[]");
			f->writeLine("scheme = model");
			f->writeLine("navmesh.glb");
			f->writeLine("[]");
			f->writeLine("scheme = collider");
			f->writeLine("collider.glb");
			f->writeLine("[]");
			f->writeLine("scheme = object");
			f->writeLine("planet.object");
			f->writeLine("[]");
			f->writeLine("scheme = pack");
			f->writeLine("planet.pack");
			f->close();
		}

		{ // generate blender import script
			Holder<File> f = writeFile(pathJoin(assetsDirectory, "blender-import.py"));
			f->write(R"Python(#!blender -y -P
import os
import bpy

def loadChunk(meshname):
	bpy.ops.import_scene.gltf(filepath = meshname)

)Python");
			for (const Chunk &c : chunks)
				f->writeLine(Stringizer() + "loadChunk('" + c.mesh + "')");
			f->write(R"Python(
for a in bpy.data.window_managers[0].windows[0].screen.areas:
	if a.type == 'VIEW_3D':
		for s in a.spaces:
			if s.type == 'VIEW_3D':
				s.clip_start = 2
				s.clip_end = 200000
				s.shading.type = 'MATERIAL'

bpy.ops.object.select_all(action='DESELECT')
)Python");
		}
	}

	struct NavmeshProcessor
	{
		Holder<AsyncTask> taskRef;
		Holder<Mesh> base;

		void taskNavmesh(uint32)
		{
			Holder<Mesh> navmesh = base->copy();
			meshSimplifyNavmesh(navmesh);
			CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "navmesh tiles: " + navmesh->verticesCount());
			std::vector<Tile> tiles;
			generateTileProperties(navmesh, tiles, pathJoin(baseDirectory, "tileStats.log"));
			meshSaveNavigation(pathJoin(assetsDirectory, "navmesh.glb"), navmesh, tiles);
			generateDoodads(navmesh, tiles, assetPackages, pathJoin(baseDirectory, "doodads.ini"), pathJoin(baseDirectory, "doodadStats.log"));
			generateStartingPositions(navmesh, tiles, pathJoin(baseDirectory, "starts.ini"));
		}

		void taskCollider(uint32)
		{
			Holder<Mesh> collider = base->copy();
			meshSimplifyCollider(collider);
			meshSaveCollider(pathJoin(assetsDirectory, "collider.glb"), collider);
		}

		void processEntry(uint32)
		{
			base = meshGenerateBaseNavigation();
			if (configDebugSaveIntermediate)
				meshSaveDebug(pathJoin(debugDirectory, "navMeshBase.glb"), base);
			Holder<AsyncTask> tn = tasksRunAsync("navmesh", Delegate<void(uint32)>().bind<NavmeshProcessor, &NavmeshProcessor::taskNavmesh>(this), 1, tasksCurrentPriority());
			Holder<AsyncTask> tc = tasksRunAsync("collider", Delegate<void(uint32)>().bind<NavmeshProcessor, &NavmeshProcessor::taskCollider>(this), 1, tasksCurrentPriority());
			tn->wait();
			tc->wait();
		}

		NavmeshProcessor()
		{
			taskRef = tasksRunAsync("navmesh", Delegate<void(uint32)>().bind<NavmeshProcessor, &NavmeshProcessor::processEntry>(this), 1, 30);
		}

		void wait()
		{
			taskRef->wait();
		}
	};

	struct LandProcessor
	{
		std::vector<Holder<Mesh>> split;

		Holder<AsyncTask> taskRef;

		void chunkEntry(uint32 index)
		{
			Chunk c;
			c.mesh = Stringizer() + "land-" + index + ".glb";
			c.albedo = Stringizer() + "land-" + index + "-albedo.png";
			c.pbr = Stringizer() + "land-" + index + "-pbr.png";
			c.normal = Stringizer() + "land-" + index + "-normal.png";
			const auto &msh = split[index];
			const uint32 resolution = meshUnwrap(msh);
			meshSaveRender(pathJoin(assetsDirectory, c.mesh), msh, c.transparency);
			Holder<Image> albedo, special, heightMap;
			generateTexturesLand(msh, resolution, resolution, albedo, special, heightMap);
			albedo->exportFile(pathJoin(assetsDirectory, c.albedo));
			imageConvertSpecialToGltfPbr(+special);
			special->exportFile(pathJoin(assetsDirectory, c.pbr));
			imageConvertHeigthToNormal(+heightMap, 1);
			heightMap->exportFile(pathJoin(assetsDirectory, c.normal));
			{
				ScopeLock lock(chunksMutex);
				chunks.push_back(c);
			}
		}

		void processEntry(uint32)
		{
			{
				Holder<Mesh> mesh = meshGenerateBaseLand();
				if (configDebugSaveIntermediate)
					meshSaveDebug(pathJoin(debugDirectory, "landMeshBase.glb"), mesh);
				meshSimplifyRender(mesh);
				if (configDebugSaveIntermediate)
					meshSaveDebug(pathJoin(debugDirectory, "landMeshSimplified.glb"), mesh);
				split = meshSplit(mesh);
				CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "land mesh split into " + split.size() + " chunks");
			}
			tasksRunBlocking("land chunk", Delegate<void(uint32)>().bind<LandProcessor, &LandProcessor::chunkEntry>(this), numeric_cast<uint32>(split.size()), tasksCurrentPriority());
		}

		LandProcessor()
		{
			taskRef = tasksRunAsync("land", Delegate<void(uint32)>().bind<LandProcessor, &LandProcessor::processEntry>(this), 1, 20);
		}

		void wait()
		{
			taskRef->wait();
		}
	};

	struct WaterProcessor
	{
		std::vector<Holder<Mesh>> split;

		Holder<AsyncTask> taskRef;

		void chunkEntry(uint32 index)
		{
			Chunk c;
			c.mesh = Stringizer() + "water-" + index + ".glb";
			c.albedo = Stringizer() + "water-" + index + "-albedo.png";
			c.pbr = Stringizer() + "water-" + index + "-pbr.png";
			c.normal = Stringizer() + "water-" + index + "-normal.png";
			c.transparency = true;
			const auto &msh = split[index];
			const uint32 resolution = meshUnwrap(msh);
			meshSaveRender(pathJoin(assetsDirectory, c.mesh), msh, c.transparency);
			Holder<Image> albedo, special, heightMap;
			generateTexturesWater(msh, resolution, resolution, albedo, special, heightMap);
			albedo->exportFile(pathJoin(assetsDirectory, c.albedo));
			imageConvertSpecialToGltfPbr(+special);
			special->exportFile(pathJoin(assetsDirectory, c.pbr));
			imageConvertHeigthToNormal(+heightMap, 1);
			heightMap->exportFile(pathJoin(assetsDirectory, c.normal));
			{
				ScopeLock lock(chunksMutex);
				chunks.push_back(c);
			}
		}

		void processEntry(uint32)
		{
			{
				Holder<Mesh> mesh = meshGenerateBaseWater();
				if (mesh->indicesCount() == 0)
				{
					CAGE_LOG(SeverityEnum::Info, "generator", "generated no water");
					return;
				}
				if (configDebugSaveIntermediate)
					meshSaveDebug(pathJoin(debugDirectory, "waterMeshBase.glb"), mesh);
				meshSimplifyRender(mesh);
				if (configDebugSaveIntermediate)
					meshSaveDebug(pathJoin(debugDirectory, "waterMeshSimplified.glb"), mesh);
				split = meshSplit(mesh);
				CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "water mesh split into " + split.size() + " chunks");
			}
			tasksRunBlocking("water chunk", Delegate<void(uint32)>().bind<WaterProcessor, &WaterProcessor::chunkEntry>(this), numeric_cast<uint32>(split.size()), tasksCurrentPriority());
		}

		WaterProcessor()
		{
			taskRef = tasksRunAsync("water", Delegate<void(uint32)>().bind<WaterProcessor, &WaterProcessor::processEntry>(this), 1, 10);
		}

		void wait()
		{
			taskRef->wait();
		}
	};
}

void generateEntry()
{
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "planet name: '" + planetName + "'");
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "tmp directory: " + baseDirectory);

	terrainPreseed();

	{
		NavmeshProcessor navigation;
		LandProcessor land;
		WaterProcessor water;
		navigation.wait();
		land.wait();
		water.wait();
	}

	exportConfiguration();

	const String outDirectory = findOutputDirectory(planetName);
	CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "output directory: " + outDirectory);
	pathMove(baseDirectory, outDirectory);

	if (configPreviewEnable)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "starting preview");
		try
		{
			ProcessCreateConfig cfg("blender -y -P blender-import.py", pathJoin(outDirectory, "data"));
			Holder<Process> p = newProcess(cfg);
			{
				detail::OverrideBreakpoint ob;
				while (true)
					CAGE_LOG(SeverityEnum::Note, "blender", p->readLine());
			}
		}
		catch (const ProcessPipeEof &)
		{
			// nothing
		}
		catch (...)
		{
			CAGE_LOG(SeverityEnum::Error, "generator", "preview failure:");
			detail::logCurrentCaughtException();
		}
	}

	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}
