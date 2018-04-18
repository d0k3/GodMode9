#include <stddef.h>
#include <stdint.h>

#include "lodepng.h"
#include "png.h"

#ifndef MONITOR_HEAP
static inline void _rgb_swap(u8 *img, size_t sz)
{
	// maybe process in batches of 3 pixels / 12 bytes at a time?
	for (size_t i = 0; i < sz; i+=3) {
		u8 c = img[i];
		img[i] = img[i + 2];
		img[i + 2] = c;
	}
}

u8 *PNG_Decompress(const u8 *png, size_t png_len, u32 *w, u32 *h)
{
	u8 *img;
	u32 res;
	size_t w_, h_;

	res = lodepng_decode24(&img, &w_, &h_, png, png_len);
	if (res) {
		if (img) free(img);
		return NULL;
	}
	_rgb_swap(img, w_ * h_ * 3);
	if (w) *w = w_;
	if (h) *h = h_;

	return img;
}

u8 *PNG_Compress(u8 *fb, u32 w, u32 h, size_t *png_sz)
{
	u32 res;
	size_t png_size;
	u8 *img;

	res = lodepng_encode24(&img, &png_size, fb, w, h);
	if (res) {
		if (img) free(img);
		return NULL;
	}
	if (png_sz) *png_sz = png_size;

	return img;
}
#else
u8 *PNG_Decompress(const u8 *png, size_t png_len, u32 *w, u32 *h)
{
	(void) png;
	(void) w;
	(void) h;
	(void) png_len;
	return NULL;
}

u8 *PNG_Compress(u8 *fb, u32 w, u32 h, size_t *png_sz)
{
	(void) fb;
	(void) png_sz;
	(void) w;
	(void) h;
	return NULL;
}
#endif
