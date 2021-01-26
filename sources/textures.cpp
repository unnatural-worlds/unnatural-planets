#include <cage-core/image.h>
#include <cage-core/polyhedron.h>

#include "terrain.h"
#include "generator.h"

namespace
{
	struct Generator
	{
		const Holder<Polyhedron> &mesh;
		Holder<Image> &albedo;
		Holder<Image> &special;
		Holder<Image> &heightMap;
		const uint32 width;
		const uint32 height;
		const bool water;

		Generator(const Holder<Polyhedron> &mesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap, bool water) : mesh(mesh), width(width), height(height), albedo(albedo), special(special), heightMap(heightMap), water(water)
		{}

		void pixel(uint32 x, uint32 y, const ivec3 &indices, const vec3 &weights)
		{
			Tile tile;
			tile.position = mesh->positionAt(indices, weights);
			tile.normal = mesh->normalAt(indices, weights);
			terrainTile(tile, water);
			albedo->set(x, y, tile.albedo);
			special->set(x, y, vec2(tile.roughness, tile.metallic));
			heightMap->set(x, y, tile.height);
		}

		void generate()
		{
			albedo = newImage();
			albedo->initialize(width, height, 3, ImageFormatEnum::Float);
			imageFill(+albedo, vec3::Nan());
			special = newImage();
			special->initialize(width, height, 2, ImageFormatEnum::Float);
			imageFill(+special, vec2::Nan());
			heightMap = newImage();
			heightMap->initialize(width, height, 1, ImageFormatEnum::Float);
			imageFill(+heightMap, real::Nan());

			{
				PolyhedronTextureGenerationConfig cfg;
				cfg.width = width;
				cfg.height = height;
				cfg.generator.bind<Generator, &Generator::pixel>(this);
				polyhedronGenerateTexture(+mesh, cfg);
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

void generateTexturesLand(const Holder<Polyhedron> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap)
{
	Generator gen(renderMesh, width, height, albedo, special, heightMap, false);
	gen.generate();
}

void generateTexturesWater(const Holder<Polyhedron> &renderMesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap)
{
	Generator gen(renderMesh, width, height, albedo, special, heightMap, true);
	gen.generate();
}
