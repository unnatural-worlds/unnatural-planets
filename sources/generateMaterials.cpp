#include <cage-core/image.h>

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

		Generator(const Holder<Polyhedron> &mesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap) : mesh(mesh), width(width), height(height), albedo(albedo), special(special), heightMap(heightMap)
		{}

		void pixel(uint32 x, uint32 y, const ivec3 &indices, const vec3 &weights)
		{
			const vec3 position = mesh->positionAt(indices, weights);
			const vec3 normal = mesh->normalAt(indices, weights);
			vec3 a;
			vec2 s;
			real h;
			functionMaterial(position, normal, a, s, h);
			albedo->set(x, y, a);
			special->set(x, y, s);
			heightMap->set(x, y, h);
		}

		void generate()
		{
			albedo = newImage();
			albedo->initialize(width, height, 3, ImageFormatEnum::Float);
			albedo->fill(vec3::Nan());
			special = newImage();
			special->initialize(width, height, 2, ImageFormatEnum::Float);
			special->fill(vec2::Nan());
			heightMap = newImage();
			heightMap->initialize(width, height, 1, ImageFormatEnum::Float);
			heightMap->fill(real::Nan());

			{
				OPTICK_EVENT("generate textures");
				PolyhedronTextureGenerationConfig cfg;
				cfg.width = width;
				cfg.height = height;
				cfg.generator.bind<Generator, &Generator::pixel>(this);
				mesh->generateTexture(cfg);
			}

			{
				OPTICK_EVENT("inpaint");
				albedo->inpaint(7, true);
				special->inpaint(7, true);
				heightMap->inpaint(7, true);
			}

			albedo->convert(ImageFormatEnum::U8);
			special->convert(ImageFormatEnum::U8);
			heightMap->convert(ImageFormatEnum::U8);

			albedo->verticalFlip();
			special->verticalFlip();
			heightMap->verticalFlip();
		}
	};
}

void generateMaterials(const Holder<Polyhedron> &mesh, uint32 width, uint32 height, Holder<Image> &albedo, Holder<Image> &special, Holder<Image> &heightMap)
{
	OPTICK_EVENT();

	Generator gen(mesh, width, height, albedo, special, heightMap);
	gen.generate();
}



