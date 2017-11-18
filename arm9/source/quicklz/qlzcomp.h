#pragma once

#include "common.h"

u32 QlzCompress(void* out, const void* in, u32 size_decomp);
u32 QlzDecompress(void* out, const void* in, u32 size);
