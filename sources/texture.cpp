#include "texture.h"

#include <utility>
#include <atomic>

void textureFill(Image *img, real value)
{
	uint32 w = img->width();
	uint32 h = img->height();
	uint32 cc = img->channels();
	for (uint32 y = 0; y < h; y++)
		for (uint32 x = 0; x < w; x++)
			for (uint32 c = 0; c < cc; c++)
				img->value(x, y, c, value);
}

namespace
{
	template<class T>
	void inpaintProcess(Image *src, Image *dst)
	{
		OPTICK_EVENT();
		uint32 w = src->width();
		uint32 h = src->height();
		for (uint32 y = 0; y < h; y++)
		{
			for (uint32 x = 0; x < w; x++)
			{
				T m;
				src->get(x, y, m);
				if (!valid(m))
				{
					m = T();
					uint32 cnt = 0;
					uint32 sy = numeric_cast<uint32>(clamp(sint32(y) - 1, 0, sint32(h) - 1));
					uint32 ey = numeric_cast<uint32>(clamp(sint32(y) + 1, 0, sint32(h) - 1));
					uint32 sx = numeric_cast<uint32>(clamp(sint32(x) - 1, 0, sint32(w) - 1));
					uint32 ex = numeric_cast<uint32>(clamp(sint32(x) + 1, 0, sint32(w) - 1));
					for (uint32 yy = sy; yy <= ey; yy++)
					{
						for (uint32 xx = sx; xx <= ex; xx++)
						{
							T a;
							src->get(xx, yy, a);
							if (valid(a))
							{
								m += a;
								cnt++;
							}
						}
					}
					if (cnt > 0)
						dst->set(x, y, m / cnt);
				}
				else
					dst->set(x, y, m);
			}
		}
	}

	template<class T>
	void inpaintProcess(Image *img, uint32 rounds)
	{
		CAGE_ASSERT(img);
		CAGE_ASSERT(img->format() == ImageFormatEnum::Float);

		Holder<Image> tmp = newImage();
		tmp->empty(img->width(), img->height(), img->channels(), img->format());
		textureFill(tmp.get(), real::Nan());

		Image *src = img;
		Image *dst = tmp.get();

		for (uint32 r = 0; r < rounds; r++)
		{
			inpaintProcess<T>(src, dst);
			std::swap(src, dst);
		}

		if (dst != img)
			imageBlit(dst, img, 0, 0, 0, 0, img->width(), img->height());
	}
}

void textureInpaint(Image *img, uint32 rounds)
{
	OPTICK_EVENT();
	switch (img->channels())
	{
	case 1: inpaintProcess<real>(img, rounds); break;
	case 2: inpaintProcess<vec2>(img, rounds); break;
	case 3: inpaintProcess<vec3>(img, rounds); break;
	case 4: inpaintProcess<vec4>(img, rounds); break;
	}
}
