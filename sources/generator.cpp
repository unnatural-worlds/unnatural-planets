#include <cage-core/threadPool.h>
#include <cage-core/concurrent.h>
#include <cage-core/files.h>
#include <cage-core/config.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/polyhedron.h>
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
			string name = pathReplaceInvalidCharacters(planetName);
			name = replace(name, " ", "_");
#ifdef CAGE_DEBUG
			name += "_debug";
#endif // CAGE_DEBUG
			const string pth = pathJoin(root, name);
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

	const string planetName = generateName();
	const string baseDirectory = findTmpDirectory();
	const string assetsDirectory = pathJoin(baseDirectory, "data");
	const string debugDirectory = pathJoin(baseDirectory, "intermediate");
	ConfigString configShapeMode("unnatural-planets/shape/mode");
	ConfigBool configDebugSaveIntermediate("unnatural-planets/debug/saveIntermediate");
	ConfigBool configPreviewEnable("unnatural-planets/preview/enable");
	std::vector<string> assetPackages;
	struct Chunk
	{
		string mesh;
		string material;
		string albedo, special, heightmap;
		bool transparency = false;
	};
	std::vector<Chunk> chunks;
	Holder<Mutex> chunksMutex = newMutex();
	Holder<Mutex> renderMutex = newMutex(); // prevent rendering land and water simultaneously (avoid exhausting resources)

	void exportConfiguration()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");

		{ // write unnatural-map
			Holder<File> f = writeFile(pathJoin(baseDirectory, "unnatural-map.ini"));
			f->writeLine("[map]");
			f->writeLine(stringizer() + "name = " + planetName);
			f->writeLine("version = 0");
			f->writeLine("[description]");
			f->writeLine(configShapeMode);
			{
				const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				char buffer[50];
				std::strftime(buffer, 50, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
				f->writeLine(stringizer() + "date: " + buffer);
			}
#ifdef CAGE_DEBUG
			f->writeLine("generated with DEBUG build");
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
			for (const Chunk &c : chunks)
				if (!c.special.empty())
					f->writeLine(c.special);
			f->writeLine("[]");
			f->writeLine("scheme = texture");
			f->writeLine("convert = heightToNormal");
			for (const Chunk &c : chunks)
				if (!c.heightmap.empty())
					f->writeLine(c.heightmap);
			for (const Chunk &c : chunks)
			{
				f->writeLine("[]");
				f->writeLine("scheme = mesh");
				f->writeLine("tangents = true");
				f->writeLine("instancesLimit = 1");
				f->writeLine(stringizer() + "material = " + c.material);
				f->writeLine(c.mesh);
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

		{ // generate blender import script
			Holder<File> f = writeFile(pathJoin(assetsDirectory, "blender-import.py"));
			f->write(R"Python(
import os
import bpy

def loadChunk(meshname, objname, albedoname, specialname, heightname, transparency):
	bpy.ops.import_scene.obj(filepath = meshname)
	bpy.ops.image.open(filepath = os.getcwd() + '/' + albedoname)
	bpy.ops.image.open(filepath = os.getcwd() + '/' + specialname)
	bpy.ops.image.open(filepath = os.getcwd() + '/' + heightname)
	mat = bpy.data.materials[objname]
	nodes = mat.node_tree.nodes
	links = mat.node_tree.links
	shader = nodes[0]
	shader.inputs['Specular'].default_value = 0.1
	albedoMap = nodes.new('ShaderNodeTexImage')
	albedoMap.image = bpy.data.images[albedoname]
	links.new(albedoMap.outputs['Color'], shader.inputs['Base Color'])
	if transparency:
		links.new(albedoMap.outputs['Alpha'], shader.inputs['Alpha'])
		mat.blend_method = 'BLEND'
	specialMap = nodes.new('ShaderNodeTexImage')
	specialMap.image = bpy.data.images[specialname]
	specialMap.image.colorspace_settings.name = 'Non-Color'
	links.new(specialMap.outputs['Color'], shader.inputs['Roughness'])
	links.new(specialMap.outputs['Alpha'], shader.inputs['Metallic'])
	heightMap = nodes.new('ShaderNodeTexImage')
	heightMap.image = bpy.data.images[heightname]
	heightMap.image.colorspace_settings.name = 'Non-Color'
	bump = nodes.new('ShaderNodeBump')
	bump.inputs['Strength'].default_value = 2
	bump.inputs['Distance'].default_value = 5
	links.new(heightMap.outputs['Color'], bump.inputs['Height'])
	links.new(bump.outputs['Normal'], shader.inputs['Normal'])
	bpy.data.objects[objname].material_slots[0].material = mat

)Python");
			for (const Chunk &c : chunks)
			{
				f->writeLine(stringizer() + "loadChunk('" + c.mesh + "', '" + replace(c.mesh, ".obj", "") + "', '"
					+ c.albedo + "', '" + c.special + "', '" + c.heightmap + "', " + (c.transparency ? "True" : "False") + ")");
			}
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
		Holder<Thread> thr;

		void processEntry()
		{
			Holder<Polyhedron> base = meshGenerateBaseNavigation();
			if (configDebugSaveIntermediate)
				meshSaveDebug(pathJoin(debugDirectory, "navmeshBase.obj"), base);
			{
				Holder<Polyhedron> navmesh = base->copy();
				meshSimplifyNavmesh(navmesh);
				CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "navmesh tiles: " + navmesh->verticesCount());
				std::vector<Tile> tiles;
				generateTileProperties(navmesh, tiles, pathJoin(baseDirectory, "tileStats.log"));
				meshSaveNavigation(pathJoin(assetsDirectory, "navmesh.obj"), navmesh, tiles);
				generateDoodads(navmesh, tiles, assetPackages, pathJoin(baseDirectory, "doodads.ini"), pathJoin(baseDirectory, "doodadStats.log"));
			}
			{
				Holder<Polyhedron> collider = base->copy();
				meshSimplifyCollider(collider);
				meshSaveCollider(pathJoin(assetsDirectory, "collider.obj"), collider);
			}
		}

		NavmeshProcessor()
		{
			thr = newThread(Delegate<void()>().bind<NavmeshProcessor, &NavmeshProcessor::processEntry>(this), "navmesh");
		}
	};

	struct LandProcessor
	{
		std::vector<Holder<Polyhedron>> split;

		Holder<Thread> thr; // make sure the thread is finished before other members are destroyed

		void chunkEntry(uint32 threadIndex, uint32 threadsCount)
		{
			uint32 b, e;
			threadPoolTasksSplit(threadIndex, threadsCount, numeric_cast<uint32>(split.size()), b, e);
			for (uint32 index = b; index < e; index++)
			{
				Chunk c;
				c.mesh = stringizer() + "land-" + index + ".obj";
				c.material = stringizer() + "land-" + index + ".cpm";
				c.albedo = stringizer() + "land-" + index + "-albedo.png";
				c.special = stringizer() + "land-" + index + "-special.png";
				c.heightmap = stringizer() + "land-" + index + "-height.png";
				const auto &msh = split[index];
				const uint32 resolution = meshUnwrap(msh);
				meshSaveRender(pathJoin(assetsDirectory, c.mesh), msh, c.transparency);
				Holder<Image> albedo, special, heightMap;
				generateTexturesLand(msh, resolution, resolution, albedo, special, heightMap);
				albedo->exportFile(pathJoin(assetsDirectory, c.albedo));
				special->exportFile(pathJoin(assetsDirectory, c.special));
				heightMap->exportFile(pathJoin(assetsDirectory, c.heightmap));
				{
					ScopeLock lock(chunksMutex);
					chunks.push_back(c);
				}
			}
		}

		void processEntry()
		{
			{
				Holder<Polyhedron> mesh = meshGenerateBaseLand();
				meshSimplifyRender(mesh);
				if (configDebugSaveIntermediate)
					meshSaveDebug(pathJoin(debugDirectory, "landMesh.obj"), mesh);
				split = meshSplit(mesh);
				CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "land mesh split into " + split.size() + " chunks");
			}
			{
				ScopeLock lock(renderMutex);
				Holder<ThreadPool> thrPool;
				thrPool = newThreadPool("land_");
				thrPool->function.bind<LandProcessor, &LandProcessor::chunkEntry>(this);
				thrPool->run();
			}
		}

		LandProcessor()
		{
			thr = newThread(Delegate<void()>().bind<LandProcessor, &LandProcessor::processEntry>(this), "land");
		}
	};

	struct WaterProcessor
	{
		std::vector<Holder<Polyhedron>> split;

		Holder<Thread> thr; // make sure the thread is finished before other members are destroyed

		void chunkEntry(uint32 threadIndex, uint32 threadsCount)
		{
			uint32 b, e;
			threadPoolTasksSplit(threadIndex, threadsCount, numeric_cast<uint32>(split.size()), b, e);
			for (uint32 index = b; index < e; index++)
			{
				Chunk c;
				c.mesh = stringizer() + "water-" + index + ".obj";
				c.material = stringizer() + "water-" + index + ".cpm";
				c.albedo = stringizer() + "water-" + index + "-albedo.png";
				c.special = stringizer() + "water-" + index + "-special.png";
				c.heightmap = stringizer() + "water-" + index + "-height.png";
				c.transparency = true;
				const auto &msh = split[index];
				const uint32 resolution = meshUnwrap(msh);
				meshSaveRender(pathJoin(assetsDirectory, c.mesh), msh, c.transparency);
				Holder<Image> albedo, special, heightMap;
				generateTexturesWater(msh, resolution, resolution, albedo, special, heightMap);
				albedo->exportFile(pathJoin(assetsDirectory, c.albedo));
				special->exportFile(pathJoin(assetsDirectory, c.special));
				heightMap->exportFile(pathJoin(assetsDirectory, c.heightmap));
				{
					ScopeLock lock(chunksMutex);
					chunks.push_back(c);
				}
			}
		}

		void processEntry()
		{
			{
				Holder<Polyhedron> mesh = meshGenerateBaseWater();
				if (mesh->indicesCount() == 0)
				{
					CAGE_LOG(SeverityEnum::Info, "generator", "generated no water");
					return;
				}
				meshSimplifyRender(mesh);
				if (configDebugSaveIntermediate)
					meshSaveDebug(pathJoin(debugDirectory, "waterMesh.obj"), mesh);
				split = meshSplit(mesh);
				CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "water mesh split into " + split.size() + " chunks");
			}
			{
				ScopeLock lock(renderMutex);
				Holder<ThreadPool> thrPool;
				thrPool = newThreadPool("water_");
				thrPool->function.bind<WaterProcessor, &WaterProcessor::chunkEntry>(this);
				thrPool->run();
			}
		}

		WaterProcessor()
		{
			thr = newThread(Delegate<void()>().bind<WaterProcessor, &WaterProcessor::processEntry>(this), "water");
		}
	};
}

void generateEntry()
{
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "planet name: '" + planetName + "'");
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "tmp directory: " + baseDirectory);

	terrainPreseed();

	{
		NavmeshProcessor navigation;
		LandProcessor land;
		WaterProcessor water;
	}

	exportConfiguration();

	const string outDirectory = findOutputDirectory(planetName);
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "output directory: " + outDirectory);
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
