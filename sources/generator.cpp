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

		void processEntry(uint32 index, uint32)
		{
			const auto &msh = unwrap->meshes[index];
			msh->validate();
			Holder<Image> albedo, special, heightMap;
			generateMaterials(msh, unwrap->textureWidth, unwrap->textureHeight, albedo, special, heightMap);
			{
				OPTICK_EVENT("inpainting");
				for (uint32 i = 0; i < 7; i++)
					albedo = textureInpaint(albedo);
				for (uint32 i = 0; i < 7; i++)
					special = textureInpaint(special);
				for (uint32 i = 0; i < 7; i++)
					heightMap = textureInpaint(heightMap);
			}
			saveRenderMesh(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + ".obj"), msh);
			albedo->encodeFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-albedo.png"));
			special->encodeFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-special.png"));
			heightMap->encodeFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-height.png"));
		}

		void process()
		{
			thrPool = newThreadPool("process_", unwrap->meshes.size());
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

	void exportConfiguration(const string &baseDirectory, uint32 renderChunksCount)
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "exporting");
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "target directory: " + pathToAbs(baseDirectory));
		OPTICK_EVENT("exportTerrain");

		const string assetsDirectory = pathJoin(baseDirectory, "data");

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

		CAGE_LOG(SeverityEnum::Info, "generator", "exported ok");
	}
}

void generateEntry()
{
	const string baseDirectory = findBaseDirectory();
	const string assetsDirectory = pathJoin(baseDirectory, "data");
	Holder<UPMesh> baseMesh = generateBaseMesh(250, 200);
	baseMesh = meshDiscardDisconnected(baseMesh);
	baseMesh->validate();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "base mesh: average edge length: " + meshAverageEdgeLength(baseMesh));
	Holder<UPMesh> navMesh = meshSimplifyRegular(baseMesh);
	navMesh->validate();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "navmesh: vertices: " + navMesh->positions.size() + ", triangles: " + (navMesh->indices.size() / 3));
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "navmesh: average edge length: " + meshAverageEdgeLength(navMesh));
	Holder<UPMesh> colliderMesh = meshSimplifyDynamic(navMesh);
	colliderMesh->validate();
	CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "collider: vertices: " + colliderMesh->positions.size() + ", triangles: " + (colliderMesh->indices.size() / 3));
	UnwrapResult unwrap = meshUnwrap(colliderMesh);
	exportConfiguration(baseDirectory, numeric_cast<uint32>(unwrap.meshes.size()));
	{
		CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "processing individual atlases");
		SubmeshProcessor submeshProcessor;
		submeshProcessor.assetsDirectory = assetsDirectory;
		submeshProcessor.unwrap = &unwrap;
		submeshProcessor.process();
	}
	std::vector<uint8> terrainTypes = generateTileProperties(navMesh);
	saveNavigationMesh(pathJoin(assetsDirectory, "navmesh.obj"), navMesh, terrainTypes);
	saveCollider(pathJoin(assetsDirectory, "collider.obj"), colliderMesh);
	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}

