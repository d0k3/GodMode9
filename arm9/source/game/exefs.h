#pragma once

#include "common.h"

typedef struct {
    char name[8];
    u32  offset;
    u32  size;
} __attribute__((packed)) ExeFsFileHeader;

// see: https://www.3dbrew.org/wiki/ExeFS
typedef struct {
    ExeFsFileHeader files[10];
    u8 reserved[0x20];
    u8 hashes[10][0x20];
} __attribute__((packed)) ExeFsHeader;

u32 ValidateExeFsHeader(ExeFsHeader* exefs, u32 size);
