#pragma once

#include "common.h"

#define THREEDSX_MAGIC      '3', 'D', 'S', 'X'
#define THREEDSX_EXT_MAGIC  THREEDSX_MAGIC, 0x2C, 0x00


// see: http://3dbrew.org/wiki/3DSX_Format
typedef struct {
    u8  magic[4]; // "3DSX"
    u16 size_hdr; // 0x2C with extended header
    u16 size_reloc_hdr; // 0x08 if existing
    u32 version; // should be zero
    u32 flags; // should be zero, too
    u32 size_code;
    u32 size_rodata;
    u32 size_data_bss;
    u32 size_bss;
    u32 offset_smdh;
    u32 size_smdh;
    u32 offset_romfs_lv3;
} __attribute__((packed)) ThreedsxHeader;
