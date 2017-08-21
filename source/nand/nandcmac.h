#pragma once

#include "common.h"

u32 CheckCmacPath(const char* path);
u32 ReadFileCmac(const char* path, u8* cmac);
u32 WriteFileCmac(const char* path, u8* cmac);
u32 CalculateFileCmac(const char* path, u8* cmac);
u32 CheckFileCmac(const char* path);
u32 FixFileCmac(const char* path);
u32 RecursiveFixFileCmac(const char* path);
