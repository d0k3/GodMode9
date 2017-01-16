#pragma once

#include "common.h"
#include "ff.h"

#define fvx_tell(fp) ((fp)->fptr)
#define fvx_size(fp) ((fp)->obj.objsize)

// wrapper functions for ff.h + sddata.h
// incomplete(!) extension to FatFS to support a common interface for virtual and FAT
FRESULT fvx_open (FIL* fp, const TCHAR* path, BYTE mode);
FRESULT fvx_read (FIL* fp, void* buff, UINT btr, UINT* br);
FRESULT fvx_write (FIL* fp, const void* buff, UINT btw, UINT* bw);
FRESULT fvx_close (FIL* fp);
FRESULT fvx_lseek (FIL* fp, FSIZE_t ofs);
FRESULT fvx_sync (FIL* fp);
