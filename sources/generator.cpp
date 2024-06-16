#include <algorithm>
#include <chrono>
#include <ctime>

#include "planets.h"

#include <cage-core/concurrent.h>
#include <cage-core/config.h>
#include <cage-core/debug.h>
#include <cage-core/imageAlgorithms.h>
#include <cage-core/mesh.h>
#include <cage-core/process.h>
#include <cage-core/string.h>
#include <cage-core/tasks.h>

namespace unnatural
{
	void terrainPreseed();
	bool terrainDoublesided();
	Holder<Mesh> meshGenerateBaseLand();
	Holder<Mesh> meshGenerateBaseWater();
	Holder<Mesh> meshGenerateBaseNavigation();
	Holder<PointerRange<Holder<Mesh>>> meshSplit(const Holder<Mesh> &mesh);
	void meshSimplifyCollider(Holder<Mesh> &mesh);
	void meshSimplifyNavmesh(Holder<Mesh> &mesh, const Mesh *collider);
	void meshSimplifyRender(Holder<Mesh> &mesh);
	uint32 meshUnwrap(const Holder<Mesh> &mesh);
	void meshSaveDebug(const Holder<Mesh> &mesh, const String &path);
	void meshSaveRender(const Holder<Mesh> &mesh, const String &path, bool transparency);
	void meshSaveNavigation(const Holder<Mesh> &mesh);
	void meshSaveCollider(const Holder<Mesh> &mesh);
	void generateTexturesLand(const Holder<Mesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);
	void generateTexturesWater(const Holder<Mesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap);
	void generateTileProperties(const Holder<Mesh> &navMesh);
	void generateDoodads();
	void generateStartingPositions();
	String generateName();
	void writeConfigurationDescription(File *f);

	namespace
	{
		String findTmpDirectory()
		{
			return pathToAbs(pathJoin("tmp", Stringizer() + currentProcessId()));
		}
	}

	std::vector<Tile> tiles;
	std::vector<DoodadDefinition> doodadsDefinitions;
	std::vector<String> assetPackages;
	std::vector<uint32> startingPositions;
	const String baseDirectory = findTmpDirectory();
	const String assetsDirectory = pathJoin(baseDirectory, "data");
	const String debugDirectory = pathJoin(baseDirectory, "intermediate");

	namespace
	{
		const ConfigBool configDebugSaveIntermediate("unnatural-planets/debug/saveIntermediate");
		const ConfigBool configPreviewEnable("unnatural-planets/preview/enable");
		const String planetName = generateName();

		struct Chunk
		{
			String mesh;
			String albedo, pbr, normal;
			bool transparency = false;

			void makeCpm() const
			{
				Holder<File> f = writeFile(pathJoin(assetsDirectory, mesh + "_" + pathExtractFilenameNoExtension(mesh) + ".cpm"));
				f->writeLine("[textures]");
				if (!albedo.empty())
					f->writeLine(Stringizer() + "albedo = " + albedo);
				if (!pbr.empty())
					f->writeLine(Stringizer() + "special = " + pbr);
				if (!normal.empty())
					f->writeLine(Stringizer() + "normal = " + normal);
				f->writeLine("[render]");
				if (transparency)
					f->writeLine("shader = /unnatural/core/shaders/water.glsl");
				f->writeLine("[flags]");
				if (transparency)
					f->writeLine("transparent");
				else if (terrainDoublesided())
					f->writeLine("twoSided");
				f->close();
			}
		};
		std::vector<Chunk> chunks;
		Holder<Mutex> chunksMutex = newMutex();

		struct NavmeshProcessor
		{
			Holder<AsyncTask> taskRef;

			void processEntry(uint32)
			{
				Holder<Mesh> navmesh = meshGenerateBaseNavigation();
				if (configDebugSaveIntermediate)
					meshSaveDebug(navmesh, pathJoin(debugDirectory, "navMeshBase.glb"));
				Holder<Mesh> collider = navmesh->copy();
				meshSimplifyCollider(collider);
				meshSaveCollider(collider);
				meshSimplifyNavmesh(navmesh, +collider);
				CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "navmesh tiles: " + navmesh->verticesCount());
				generateTileProperties(navmesh);
				meshSaveNavigation(navmesh);
				generateStartingPositions();
				generateDoodads();
			}

			NavmeshProcessor() { taskRef = tasksRunAsync("navmesh", Delegate<void(uint32)>().bind<NavmeshProcessor, &NavmeshProcessor::processEntry>(this)); }

			void wait() { taskRef->wait(); }
		};

		struct LandProcessor
		{
			Holder<PointerRange<Holder<Mesh>>> split;

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
				meshSaveRender(msh, pathJoin(assetsDirectory, c.mesh), c.transparency);
				Holder<Image> albedo, special, heightMap;
				generateTexturesLand(msh, resolution, resolution, albedo, special, heightMap);
				albedo->exportFile(pathJoin(assetsDirectory, c.albedo));
				imageConvertSpecialToGltfPbr(+special);
				special->exportFile(pathJoin(assetsDirectory, c.pbr));
				imageConvertHeigthToNormal(+heightMap, 1);
				heightMap->exportFile(pathJoin(assetsDirectory, c.normal));
				c.makeCpm();
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
						meshSaveDebug(mesh, pathJoin(debugDirectory, "landMeshBase.glb"));
					meshSimplifyRender(mesh);
					if (configDebugSaveIntermediate)
						meshSaveDebug(mesh, pathJoin(debugDirectory, "landMeshSimplified.glb"));
					split = meshSplit(mesh);
					CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "land mesh split into " + split.size() + " chunks");
				}
				tasksRunBlocking("land chunk", Delegate<void(uint32)>().bind<LandProcessor, &LandProcessor::chunkEntry>(this), numeric_cast<uint32>(split.size()));
			}

			LandProcessor() { taskRef = tasksRunAsync("land", Delegate<void(uint32)>().bind<LandProcessor, &LandProcessor::processEntry>(this)); }

			void wait() { taskRef->wait(); }
		};

		struct WaterProcessor
		{
			Holder<PointerRange<Holder<Mesh>>> split;

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
				meshSaveRender(msh, pathJoin(assetsDirectory, c.mesh), c.transparency);
				Holder<Image> albedo, special, heightMap;
				generateTexturesWater(msh, resolution, resolution, albedo, special, heightMap);
				albedo->exportFile(pathJoin(assetsDirectory, c.albedo));
				imageConvertSpecialToGltfPbr(+special);
				special->exportFile(pathJoin(assetsDirectory, c.pbr));
				imageConvertHeigthToNormal(+heightMap, 1);
				heightMap->exportFile(pathJoin(assetsDirectory, c.normal));
				c.makeCpm();
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
						meshSaveDebug(mesh, pathJoin(debugDirectory, "waterMeshBase.glb"));
					meshSimplifyRender(mesh);
					if (configDebugSaveIntermediate)
						meshSaveDebug(mesh, pathJoin(debugDirectory, "waterMeshSimplified.glb"));
					split = meshSplit(mesh);
					CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "water mesh split into " + split.size() + " chunks");
				}
				tasksRunBlocking("water chunk", Delegate<void(uint32)>().bind<WaterProcessor, &WaterProcessor::chunkEntry>(this), numeric_cast<uint32>(split.size()));
			}

			WaterProcessor() { taskRef = tasksRunAsync("water", Delegate<void(uint32)>().bind<WaterProcessor, &WaterProcessor::processEntry>(this)); }

			void wait() { taskRef->wait(); }
		};

		void exportConfiguration()
		{
			CAGE_LOG(SeverityEnum::Info, "generator", "exporting");

			{ // write unnatural-map
				Holder<File> f = writeFile(pathJoin(baseDirectory, "unnatural-map.ini"));
				f->writeLine("[map]");
				f->writeLine(Stringizer() + "name = " + planetName);
				f->writeLine("version = 0");

				f->writeLine("[description]");
				writeConfigurationDescription(+f);
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
				f->writeLine("unnatural-planets procedural generator");
				f->writeLine("https://github.com/unnatural-worlds/unnatural-planets");

				f->writeLine("[assets]");
				f->writeLine("pack = planet.pack");
				f->writeLine("navigation = navmesh.obj");
				f->writeLine("collider = collider.glb");

				f->writeLine("[packages]");
				f->writeLine("unnatural/base/base.pack");
				std::sort(assetPackages.begin(), assetPackages.end());
				assetPackages.erase(std::unique(assetPackages.begin(), assetPackages.end()), assetPackages.end());
				for (const String &s : assetPackages)
					f->writeLine(s);

				f->writeLine("[camera]");
				f->writeLine("position = 1300, 0, -2800");

				f->writeLine("[terrainTypes]");
				f->writeLine("road");
				f->writeLine("flat");
				f->writeLine("rough");
				f->writeLine("cliffs");
				f->writeLine("water");
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
				uint32 textureCounts[4] = {}; // opaque albedo, transparent albedo, special, normal
				for (const Chunk &c : chunks)
				{
					if (!c.albedo.empty())
						textureCounts[c.transparency]++;
					if (!c.pbr.empty())
						textureCounts[2]++;
					if (!c.normal.empty())
						textureCounts[3]++;
				}
				if (textureCounts[0])
				{
					f->writeLine("[]");
					f->writeLine("scheme = texture");
					f->writeLine("srgb = true");
					for (const Chunk &c : chunks)
						if (!c.albedo.empty() && !c.transparency)
							f->writeLine(c.albedo);
				}
				if (textureCounts[1])
				{
					f->writeLine("[]");
					f->writeLine("scheme = texture");
					f->writeLine("srgb = true");
					f->writeLine("premultiplyAlpha = true");
					for (const Chunk &c : chunks)
						if (!c.albedo.empty() && c.transparency)
							f->writeLine(c.albedo);
				}
				if (textureCounts[2])
				{
					f->writeLine("[]");
					f->writeLine("scheme = texture");
					f->writeLine("convert = gltfToSpecial");
					for (const Chunk &c : chunks)
						if (!c.pbr.empty())
							f->writeLine(c.pbr);
				}
				if (textureCounts[3])
				{
					f->writeLine("[]");
					f->writeLine("scheme = texture");
					f->writeLine("normal = true");
					for (const Chunk &c : chunks)
						if (!c.normal.empty())
							f->writeLine(c.normal);
				}
				f->writeLine("[]");
				f->writeLine("scheme = model");
				for (const Chunk &c : chunks)
					f->writeLine(c.mesh);
				f->writeLine("[]");
				f->writeLine("scheme = model");
				f->writeLine("navmesh.obj");
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

)Python");
				for (const Chunk &c : chunks)
					f->writeLine(Stringizer() + "bpy.ops.import_scene.gltf(filepath = '" + c.mesh + "')");
				f->writeLine(Stringizer() + "bpy.ops.import_scene.obj(filepath = '../starts-preview.obj')");
				f->writeLine(Stringizer() + "bpy.ops.import_scene.obj(filepath = '../doodads-preview.obj')");
				f->write(R"Python(
for a in bpy.data.window_managers[0].windows[0].screen.areas:
	if a.type == 'VIEW_3D':
		for s in a.spaces:
			if s.type == 'VIEW_3D':
				s.clip_start = 2
				s.clip_end = 200000
				s.shading.type = 'MATERIAL'
				s.region_3d.view_distance = 8000

bpy.ops.object.select_all(action='DESELECT')
)Python");
			}
		}

		String findOutputDirectory(const String &overrideOutputPath)
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
				String name = overrideOutputPath.empty() ? planetName : overrideOutputPath;
				name = pathReplaceInvalidCharacters(name);
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
	}

	void generateEntry(const String &overrideOutputPath)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "planet name: '" + planetName + "'");
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "tmp directory: " + baseDirectory);
		pathRemove(baseDirectory);

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

		const String outDirectory = findOutputDirectory(overrideOutputPath);
		CAGE_LOG(SeverityEnum::Info, "generator", Stringizer() + "output directory: " + outDirectory);
		CAGE_ASSERT(!pathIsDirectory(outDirectory) && !pathIsFile(outDirectory));
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
}
