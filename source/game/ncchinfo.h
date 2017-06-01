#pragma once

#include "common.h"

#define NCCHINFO_NAME "ncchinfo.bin"
#define NCCHINFO_V3_MAGIC 0xF0000003
#define NCCHINFO_V4_MAGIC 0xF0000004
#define NCCHINFO_V3_SIZE 160

typedef struct {
    u8   ctr[16];
    u8   keyY[16];
    u32  size_mb;
    u32  size_b; // this is only used if it is non-zero
    u32  ncchFlag7;
    u32  ncchFlag3;
    u64  titleId;
    char filename[112];
} __attribute__((packed)) NcchInfoEntry;

typedef struct {
    u32 padding;
    u32 ncch_info_version;
    u32 n_entries;
    u8  reserved[4];
} __attribute__((packed, aligned(16))) NcchInfoHeader;

u32 GetNcchInfoVersion(NcchInfoHeader* info);
u32 FixNcchInfoEntry(NcchInfoEntry* entry, u32 version);
u32 BuildNcchInfoXorpad(void* buffer, NcchInfoEntry* entry, u32 size, u32 offset);
