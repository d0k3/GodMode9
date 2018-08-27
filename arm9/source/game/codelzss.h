#pragma once

#include "common.h"

#define EXEFS_CODE_NAME  ".code"

u32 GetCodeLzssUncompressedSize(void* footer, u32 comp_size);
u32 DecompressCodeLzss(u8* code, u32* code_size, u32 max_size);
bool CompressCodeLzss(const u8* a_pUncompressed, u32 a_uUncompressedSize, u8* a_pCompressed, u32* a_uCompressedSize);
