#include "generator.h"

#include <cage-core/threadPool.h>
#include <cage-core/files.h>
#include <cage-core/random.h>
#include <cage-core/timer.h> // formatDateTime

namespace
{
	Holder<Image> albedo, special, heightMap;

	void inpaintEntry(uint32 idx, uint32)
	{
		switch (idx)
		{
		case 0:
			for (uint32 i = 0; i < 7; i++)
				albedo = textureInpaint(albedo);
			break;
		case 1:
			for (uint32 i = 0; i < 7; i++)
				special = textureInpaint(special);
			break;
		case 2:
			for (uint32 i = 0; i < 7; i++)
				heightMap = textureInpaint(heightMap);
			break;
		}
	}

	void inpaintTextures()
	{
		CAGE_LOG(SeverityEnum::Info, "generator", "inpainting textures");
		OPTICK_EVENT();

		Holder<ThreadPool> thr = newThreadPool("inpaint_", 3);
		thr->function.bind<&inpaintEntry>();
		thr->run();
	}

	string findBaseDirectory()
	{
		uint32 index = 1;
		while (true)
		{
			string pth = pathJoin("output", stringizer() + index);
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
	Holder<UPMesh> baseMesh = generateBaseMesh(150, 150);
	Holder<UPMesh> navMesh = meshSimplifyRegular(baseMesh);
	Holder<UPMesh> colliderMesh = meshSimplifyDynamic(navMesh);
	UnwrapResult unwrap = meshUnwrap(colliderMesh);
	exportConfiguration(baseDirectory, numeric_cast<uint32>(unwrap.meshes.size()));
	uint32 index = 0;
	for (Holder<UPMesh> &msh : unwrap.meshes)
	{
		generateMaterials(msh, unwrap.textureWidth, unwrap.textureHeight, albedo, special, heightMap);
		inpaintTextures();
		saveRenderMesh(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + ".obj"), msh);
		albedo->encodeFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-albedo.png"));
		special->encodeFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-special.png"));
		heightMap->encodeFile(pathJoin(assetsDirectory, stringizer() + "chunk-" + index + "-height.png"));
		index++;
	}
	std::vector<uint8> terrainTypes = generateTileProperties(navMesh);
	saveNavigationMesh(pathJoin(assetsDirectory, "navmesh.obj"), navMesh, terrainTypes);
	saveCollider(pathJoin(assetsDirectory, "collider.obj"), colliderMesh);
	CAGE_LOG(SeverityEnum::Info, "generator", "all done");
}

