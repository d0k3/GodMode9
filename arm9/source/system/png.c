#include <stddef.h>
#include <stdint.h>

#include "lodepng.h"
#include "png.h"

// dest and src can be the same
static inline void _rgb24_to_rgb565(u16 *dest, const u8 *src, size_t dim)
{
	for (size_t i = 0; i < dim; i++) {
		u8 r, g, b;

		r = *(src++) >> 3;
		g = *(src++) >> 2;
		b = *(src++) >> 3;
		*(dest++) = r << 11 | g << 5 | b;
	}
}

// dest and src CAN NOT be the same
static inline void _rgb565_to_rgb24(u8 *dest, const u16 *src, size_t dim)
{
	for (size_t i = 0; i < dim; i++) {
		u16 rgb = *(src++);

		*(dest++) = (rgb >> 11) << 3;
		*(dest++) = ((rgb >> 5) & 0x3F) << 2;
		*(dest++) = (rgb & 0x1F) << 3;
	}
}

u16 *PNG_Decompress(const u8 *png, size_t png_len, u32 *w, u32 *h)
{
	u16 *img;
	unsigned res;
	size_t width, height;

	img = NULL;
	res = lodepng_decode24((u8**)&img, &width, &height, png, png_len);
	if (res) {
		free(img);
		return NULL;
	}

	_rgb24_to_rgb565(img, (const u8*)img, width * height);
	if (w) *w = width;
	if (h) *h = height;

	// the allocated buffer will be w*h*3 bytes long, but only w*h*2 bytes will be used
	// however, this is not a problem and it'll all be freed with a regular free() call
	return (u16*)img;
}

u8 *PNG_Compress(const u16 *fb, u32 w, u32 h, size_t *png_sz)
{
	u8 *img, *buf;
	unsigned res;
	size_t png_size;

	img = NULL;

	buf = malloc(w * h * 3);
	if (!buf) return NULL;

	_rgb565_to_rgb24(buf, fb, w * h);
	res = lodepng_encode24(&img, &png_size, buf, w, h);
	free(buf);

	if (res) {
		free(img);
		return NULL;
	}

	if (png_sz)
		*png_sz = png_size;

	return img;
}
