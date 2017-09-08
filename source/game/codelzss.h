#pragma once

#include "common.h"

#define EXEFS_CODE_NAME  ".code"

u32 DecompressCodeLzss(u8* data_start, u32* code_size, u32 max_size);
