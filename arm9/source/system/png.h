#pragma once

#include <stdint.h>
#include "common.h"
#include "lodepng/lodepng.h"

u8 *PNG_Decompress(const u8 *png, size_t png_len, size_t *w, size_t *h);
