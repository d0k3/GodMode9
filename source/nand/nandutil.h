#pragma once

#include "common.h"

#define TWL_OFFSET      0x000000
#define CTR_OFFSET      0x05C980
#define FIRM_OFFSETS    0x058980, 0x05A980

#define SAFE_SECTORS    0x000001, 0x000096, 0x000097, 0x058980, 0x05C980, 0x000000 // last one is a placeholder

u32 ValidateNandDump(const char* path);
u32 SafeRestoreNandDump(const char* path);
