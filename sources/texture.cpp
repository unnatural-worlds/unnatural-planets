#include "texture.h"

namespace
{
	void get(const Holder<Image> &img, uint32 x, uint32 y, real &result) { result = img->get1(x, y); }
	void get(const Holder<Image> &img, uint32 x, uint32 y, vec2 &result) { result = img->get2(x, y); }
	void get(const Holder<Image> &img, uint32 x, uint32 y, vec3 &result) { result = img->get3(x, y); }
	void get(const Holder<Image> &img, uint32 x, uint32 y, vec4 &result) { result = img->get4(x, y); }

	template<class T>
	void inpaintProcess(const Holder<Image> &src, const Holder<Image> &dst)
	{
		uint32 w = src->width();
		uint32 h = src->height();
		for (uint32 y = 0; y < h; y++)
		{
			for (uint32 x = 0; x < w; x++)
			{
				T m;
				get(src, x, y, m);
				if (m == T())
				{
					uint32 cnt = 0;
					uint32 sy = numeric_cast<uint32>(clamp(sint32(y) - 1, 0, sint32(h) - 1));
					uint32 ey = numeric_cast<uint32>(clamp(sint32(y) + 1, 0, sint32(h) - 1));
					uint32 sx = numeric_cast<uint32>(clamp(sint32(x) - 1, 0, sint32(w) - 1));
					uint32 ex = numeric_cast<uint32>(clamp(sint32(x) + 1, 0, sint32(w) - 1));
					T a;
					for (uint32 yy = sy; yy <= ey; yy++)
					{
						for (uint32 xx = sx; xx <= ex; xx++)
						{
							get(src, xx, yy, a);
							if (a != T())
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
}

Holder<Image> textureInpaint(const Holder<Image> &img)
{
	if (!img)
		return {};

	OPTICK_EVENT();

	uint32 c = img->channels();
	Holder<Image> tmp = newImage();
	tmp->empty(img->width(), img->height(), c, img->format());
	switch (c)
	{
	case 1: inpaintProcess<real>(img, tmp); break;
	case 2: inpaintProcess<vec2>(img, tmp); break;
	case 3: inpaintProcess<vec3>(img, tmp); break;
	case 4: inpaintProcess<vec4>(img, tmp); break;
	}
	return templates::move(tmp);
}
