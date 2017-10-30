#pragma once

#include "common.h"
#include "tar.h"


#define VRAM0_OFFSET    0x18000000
#define VRAM0_LIMIT     0x00300000

#define TARDATA         ((void*) VRAM0_OFFSET)
#define TARDATA_(off)   ((void*) (u32) (VRAM0_OFFSET + (off)))
#define TARDATA_END     TARDATA_(VRAM0_LIMIT)


#define CheckVram0Tar() \
    (ValidateTarHeader(TARDATA, TARDATA_END) == 0)

#define FirstVTarEntry() \
    TARDATA
    
#define OffsetVTarEntry(off) \
    TARDATA_(off)

#define NextVTarEntry(tardata) \
    NextTarEntry(tardata, TARDATA_END)
    
#define GetVTarFileInfo(tardata, fname, fsize, is_dir) \
    GetTarFileInfo(tardata, fname, fsize, is_dir)
    
#define FindVTarFileInfo(fname, fsize, is_dir) \
    FindTarFileInfo(TARDATA, TARDATA_END, fname, fsize, is_dir)
