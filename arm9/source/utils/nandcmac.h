#pragma once

#include "common.h"

#define ReadFileCmac(path, cmac)    ReadWriteFileCmac(path, cmac, false)
#define WriteFileCmac(path, cmac)   ReadWriteFileCmac(path, cmac, true)
#define CheckCmdCmac(path)          CheckFixCmdCmac(path, false)
#define FixCmdCmac(path)            CheckFixCmdCmac(path, true)

u32 CheckCmacPath(const char* path);
u32 ReadWriteFileCmac(const char* path, u8* cmac, bool do_write);
u32 CalculateFileCmac(const char* path, u8* cmac);
u32 CheckFileCmac(const char* path);
u32 FixFileCmac(const char* path);
u32 FixAgbSaveCmac(void* data, u8* cmac, const char* sddrv);
u32 CheckFixCmdCmac(const char* path, bool fix);
u32 RecursiveFixFileCmac(const char* path);
