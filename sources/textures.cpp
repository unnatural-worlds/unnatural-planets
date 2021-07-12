#include <cage-core/image.h>
#include <cage-core/mesh.h>

#include "terrain.h"
#include "generator.h"

namespace
{
	template<bool Water>
	struct Generator
	{
		const Holder<Mesh> &mesh;
		Holder<Image> &albedo;
		Holder<Image> &special;
		Holder<Image> &heightMap;
		const uint32 width;
		const uint32 height;

		Generator(const Holder<Mesh> &mesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap) : mesh(mesh), width(width), height(height), albedo(albedo), special(special), heightMap(heightMap)
		{}

		void pixel(const ivec2 &xy, const ivec3 &indices, const vec3 &weights)
		{
			Tile tile;
			tile.position = mesh->positionAt(indices, weights);
			tile.normal = mesh->normalAt(indices, weights);
			if (Water)
			{
				terrainTileWater(tile);
				albedo->set(xy, vec4(tile.albedo, tile.opacity));
			}
			else
			{
				terrainTileLand(tile);
				albedo->set(xy, tile.albedo);
			}
			special->set(xy, vec2(tile.roughness, tile.metallic));
			heightMap->set(xy, tile.height);
		}

		void generate()
		{
			albedo = newImage();
			if (Water)
			{
				albedo->initialize(width, height, 4, ImageFormatEnum::Float);
				imageFill(+albedo, vec4::Nan());
			}
			else
			{
				albedo->initialize(width, height, 3, ImageFormatEnum::Float);
				imageFill(+albedo, vec3::Nan());
			}
			special = newImage();
			special->initialize(width, height, 2, ImageFormatEnum::Float);
			imageFill(+special, vec2::Nan());
			heightMap = newImage();
			heightMap->initialize(width, height, 1, ImageFormatEnum::Float);
			imageFill(+heightMap, real::Nan());

			{
				MeshGenerateTextureConfig cfg;
				cfg.width = width;
				cfg.height = height;
				cfg.generator.bind<Generator, &Generator::pixel>(this);
				meshGenerateTexture(+mesh, cfg);
			}

			{
				imageDilation(+albedo, 7, true);
				imageDilation(+special, 7, true);
				imageDilation(+heightMap, 7, true);
			}

			imageConvert(+albedo, ImageFormatEnum::U8);
			imageConvert(+special, ImageFormatEnum::U8);
			imageConvert(+heightMap, ImageFormatEnum::U8);

			imageVerticalFlip(+albedo);
			imageVerticalFlip(+special);
			imageVerticalFlip(+heightMap);
		}
	};
}

void generateTexturesLand(const Holder<Mesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap)
{
	Generator<false> gen(renderMesh, width, height, albedo, special, heightMap);
	gen.generate();
}

void generateTexturesWater(const Holder<Mesh> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap)
{
	Generator<true> gen(renderMesh, width, height, albedo, special, heightMap);
	gen.generate();
}
