#include <stddef.h>
#include <stdint.h>

#include "lodepng.h"
#include "png.h"

#include "ui.h"

u8 *PNG_Decompress(const u8 *png, size_t png_len, size_t *w, size_t *h)
{
	u8 *img;
	u32 res;
	size_t w_, h_;

	res = lodepng_decode24(&img, &w_, &h_, png, png_len);
	if (res) {
		ShowPrompt(false, "PNG error: %s", lodepng_error_text(res));
		return NULL;
	}

	// maybe process in batches of 3 pixels / 12 bytes at a time?
	for (size_t i = 0; i < (w_ * h_ * 3); i += 3) {
		u8 c = img[i];
		img[i] = img[i + 2];
		img[i + 2] = c;
	}

	if (w) *w = w_;
	if (h) *h = h_;

	return img;
}
