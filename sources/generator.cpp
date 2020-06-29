#include "generator.h"

#include <cage-core/threadPool.h>
#include <cage-core/files.h>
#include <cage-core/random.h>
#include <cage-core/timer.h> // formatDateTime

namespace
{
	struct SubmeshProcessor
	{
		Holder<ThreadPool> thrPool;
		const UnwrapResult *unwrap = nullptr;
		string assetsDirectory;

		void processOne(uint32 index)
		{
			CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "generating chunk " + index);
			const auto &msh = unwrap->meshes[index];
			msh->validate();
			saveRenderMesh(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + ".obj"), msh);
			Holder<Image> albedo, special, heightMap;
			generateMaterials(msh, unwrap->textureWidth, unwrap->textureHeight, albedo, special, heightMap);
			{
				OPTICK_EVENT("inpainting");
				textureInpaint(albedo.get(), 7);
				textureInpaint(special.get(), 7);
				textureInpaint(heightMap.get(), 7);
			}
			albedo->convert(ImageFormatEnum::U8);
			special->convert(ImageFormatEnum::U8);
			heightMap->convert(ImageFormatEnum::U8);
			albedo->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-albedo.png"));
			special->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-special.png"));
			heightMap->exportFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-height.png"));
		}

		void processEntry(uint32 threadIndex, uint32 threadsCount)
		{
			uint32 b, e;
			threadPoolTasksSplit(threadIndex, threadsCount, numeric_cast<uint32>(unwrap->meshes.size()), b, e);
			for (uint32 i = b; i < e; i++)
				processOne(i);
		}

		void process()
		{
			thrPool = newThreadPool("process_");
			thrPool->function.bind<SubmeshProcessor, &SubmeshProcessor::processEntry>(this);
			thrPool->run();
		}
	};

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

	void exportConfiguration(const string &baseDirectory, const string &assetsDirectory, uint32 renderChunksCount)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");
		OPTICK_EVENT("exportTerrain");

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
}

void generateEntry()
{
	const string baseDirectory = findBaseDirectory();
	const string assetsDirectory = pathJoin(baseDirectory, "data");
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "target directory: " + pathToAbs(baseDirectory));
	Holder<UPMesh> baseMesh = generateBaseMesh(250, 200);
	baseMesh = meshDiscardDisconnected(baseMesh);
	baseMesh->validate();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "base mesh: average edge length: " + meshAverageEdgeLength(baseMesh));
	Holder<UPMesh> navMesh = meshSimplifyRegular(baseMesh);
	navMesh->validate();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "navmesh: vertices: " + navMesh->positions.size() + ", triangles: " + (navMesh->indices.size() / 3));
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "navmesh: average edge length: " + meshAverageEdgeLength(navMesh));
	std::vector<uint8> terrainTypes = generateTileProperties(navMesh);
	saveNavigationMesh(pathJoin(assetsDirectory, "navmesh.obj"), navMesh, terrainTypes);
	Holder<UPMesh> colliderMesh = meshSimplifyDynamic(navMesh);
	navMesh.clear();
	colliderMesh->validate();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "collider: vertices: " + colliderMesh->positions.size() + ", triangles: " + (colliderMesh->indices.size() / 3));
	saveCollider(pathJoin(assetsDirectory, "collider.obj"), colliderMesh);
	UnwrapResult unwrap = meshUnwrap(colliderMesh);
	colliderMesh.clear();
	exportConfiguration(baseDirectory, assetsDirectory, numeric_cast<uint32>(unwrap.meshes.size()));
	{
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "processing individual meshes");
		SubmeshProcessor submeshProcessor;
		submeshProcessor.assetsDirectory = assetsDirectory;
		submeshProcessor.unwrap = &unwrap;
		submeshProcessor.process();
	}
	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}

