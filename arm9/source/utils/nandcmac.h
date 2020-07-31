#pragma once

#include "common.h"

#define ReadFileCmac(path, cmac)               ReadWriteFileCmac(path, cmac, false, true)
#define WriteFileCmac(path, cmac, check_perms) ReadWriteFileCmac(path, cmac, true, check_perms)
#define CheckCmdCmac(path)                     CheckFixCmdCmac(path, false, true)
#define FixCmdCmac(path, check_perms)          CheckFixCmdCmac(path, true, check_perms)

u32 CheckCmacPath(const char* path);
u32 ReadWriteFileCmac(const char* path, u8* cmac, bool do_write, bool check_perms);
u32 CalculateFileCmac(const char* path, u8* cmac);
u32 CheckFileCmac(const char* path);
u32 FixFileCmac(const char* path, bool check_perms);
u32 FixAgbSaveCmac(void* data, u8* cmac, const char* sddrv);
u32 CheckFixCmdCmac(const char* path, bool fix, bool check_perms);
u32 RecursiveFixFileCmac(const char* path);
