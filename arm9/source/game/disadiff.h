#pragma once

#include "common.h"


// info taken from here:
// http://3dbrew.org/wiki/DISA_and_DIFF
// https://github.com/wwylele/3ds-save-tool

#define DISA_MAGIC  'D', 'I', 'S', 'A', 0x00, 0x00, 0x04, 0x00
#define DIFF_MAGIC  'D', 'I', 'F', 'F', 0x00, 0x00, 0x03, 0x00
#define IVFC_MAGIC  'I', 'V', 'F', 'C', 0x00, 0x00, 0x02, 0x00
#define DPFS_MAGIC  'D', 'P', 'F', 'S', 0x00, 0x00, 0x01, 0x00
#define DIFI_MAGIC  'D', 'I', 'F', 'I', 0x00, 0x00, 0x01, 0x00


typedef struct {
    u8  magic[8]; // "DISA" 0x00040000
    u32 n_partitions;
    u8  padding0[4];
    u64 offset_table1;
    u64 offset_table0;
    u64 size_table;
    u64 offset_descA;
    u64 size_descA;
    u64 offset_descB;
    u64 size_descB;
    u64 offset_partitionA;
    u64 size_partitionA;
    u64 offset_partitionB;
    u64 size_partitionB;
    u8  active_table; // 0 or 1
    u8  padding1[3];
    u8  hash_table[0x20]; // for the active table
    u8  unused[0x74];
} __attribute__((packed)) DisaHeader;

typedef struct {
    u8  magic[8]; // "DIFF" 0x00030000
    u64 offset_table1; // also desc offset
    u64 offset_table0; // also desc offset
    u64 size_table; // includes desc size
    u64 offset_partition;
    u64 size_partition;
    u32 active_table; // 0 or 1
    u8  hash_table[0x20]; // for the active table
    u64 unique_id; // see: http://3dbrew.org/wiki/Extdata
    u8  unused[0xA4];
} __attribute__((packed)) DiffHeader;

typedef struct {
    u8  magic[8]; // "DIFI" 0x00010000
    u64 offset_ivfc; // always 0x44
    u64 size_ivfc; // always 0x78
    u64 offset_dpfs; // always 0xBC
    u64 size_dpfs; // always 0x50
    u64 offset_hash; // always 0x10C
    u64 size_hash; // may include padding
    u8  ivfc_use_extlvl4;
    u8  dpfs_lvl1_selector;
    u8  padding[2];
    u64 ivfc_offset_extlvl4;
} __attribute__((packed)) DifiHeader;

typedef struct {
    u8  magic[8]; // "IVFC" 0x00020000
    u64 size_hash; // same as the one in DIFI, may include padding
    u64 offset_lvl1;
    u64 size_lvl1;
    u32 log_lvl1;
    u8  padding0[4];
    u64 offset_lvl2;
    u64 size_lvl2;
    u32 log_lvl2;
    u8  padding1[4];
    u64 offset_lvl3;
    u64 size_lvl3;
    u32 log_lvl3;
    u8  padding2[4];
    u64 offset_lvl4;
    u64 size_lvl4;
    u64 log_lvl4;
    u64 size_ivfc; // 0x78
} __attribute__((packed)) IvfcDescriptor;

typedef struct {
    u8  magic[8]; // "DPFS" 0x00010000
    u64 offset_lvl1;
    u64 size_lvl1;
    u32 log_lvl1;
    u8  padding0[4];
    u64 offset_lvl2;
    u64 size_lvl2;
    u32 log_lvl2;
    u8  padding1[4];
    u64 offset_lvl3;
    u64 size_lvl3;
    u32 log_lvl3;
    u8  padding2[4];
} __attribute__((packed)) DpfsDescriptor;

typedef struct {
    DifiHeader difi;
    IvfcDescriptor ivfc;
    DpfsDescriptor dpfs;
    u8 hash[0x20];
    u8 padding[4]; // all zeroes when encrypted
} __attribute__((packed)) DifiStruct;

// condensed info to enable reading IVFC lvl4
typedef struct {
    u32 offset_dpfs_lvl1; // relative to start of file
    u32 offset_dpfs_lvl2; // relative to start of file
    u32 offset_dpfs_lvl3; // relative to start of file
    u32 size_dpfs_lvl1;
    u32 size_dpfs_lvl2;
    u32 size_dpfs_lvl3;
    u32 log_dpfs_lvl2;
    u32 log_dpfs_lvl3;
    u32 offset_ivfc_lvl4; // relative to DPFS lvl3 if not external
    u32 size_ivfc_lvl4;
    u8  dpfs_lvl1_selector;
    u8  ivfc_use_extlvl4;
    u8* dpfs_lvl2_cache; // optional, NULL when unused
} __attribute__((packed)) DisaDiffReaderInfo;

u32 GetDisaDiffReaderInfo(const char* path, DisaDiffReaderInfo* info, bool partitionB);
u32 BuildDisaDiffDpfsLvl2Cache(const char* path, DisaDiffReaderInfo* info, u8* cache, u32 cache_size);
u32 ReadDisaDiffIvfcLvl4(const char* path, DisaDiffReaderInfo* info, u32 offset, u32 size, void* buffer);
