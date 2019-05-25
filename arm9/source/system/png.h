#pragma once

#include <stdint.h>
#include "common.h"
#include "lodepng/lodepng.h"

#define PNG_MAGIC   0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A

u16 *PNG_Decompress(const u8 *png, size_t png_len, u32 *w, u32 *h);
u8 *PNG_Compress(const u16 *fb, u32 w, u32 h, size_t *png_sz);
