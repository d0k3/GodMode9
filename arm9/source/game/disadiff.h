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


// condensed info to enable reading/writing IVFC lvl4
typedef struct {
    u32 offset_table;
    u32 size_table;
    u32 offset_partition_hash;
    u32 offset_difi;
    u32 offset_master_hash; // relative to start of difi
    u32 offset_dpfs_lvl1; // relative to start of file
    u32 offset_dpfs_lvl2; // relative to start of file
    u32 offset_dpfs_lvl3; // relative to start of file
    u32 size_dpfs_lvl1;
    u32 size_dpfs_lvl2;
    u32 size_dpfs_lvl3;
    u32 log_dpfs_lvl2;
    u32 log_dpfs_lvl3;
    u32 log_ivfc_lvl1;
    u32 log_ivfc_lvl2;
    u32 log_ivfc_lvl3;
    u32 log_ivfc_lvl4;
    u32 offset_ivfc_lvl1; // relative to DPFS lvl3
    u32 offset_ivfc_lvl2; // relative to DPFS lvl3
    u32 offset_ivfc_lvl3; // relative to DPFS lvl3
    u32 offset_ivfc_lvl4; // relative to DPFS lvl3 if not external
    u32 size_ivfc_lvl1;
    u32 size_ivfc_lvl2;
    u32 size_ivfc_lvl3;
    u32 size_ivfc_lvl4;
    u8  dpfs_lvl1_selector;
    u8  ivfc_use_extlvl4;
    u8* dpfs_lvl2_cache; // optional, NULL when unused
} __attribute__((packed)) DisaDiffRWInfo;

u32 GetDisaDiffRWInfo(const char* path, DisaDiffRWInfo* info, bool partitionB);
u32 BuildDisaDiffDpfsLvl2Cache(const char* path, const DisaDiffRWInfo* info, u8* cache, u32 cache_size);
u32 ReadDisaDiffIvfcLvl4(const char* path, const DisaDiffRWInfo* info, u32 offset, u32 size, void* buffer);
u32 WriteDisaDiffIvfcLvl4(const char* path, const DisaDiffRWInfo* info, u32 offset, u32 size, const void* buffer);
// Not intended for external use other than vdisadiff
u32 FixDisaDiffIvfcLevel(const DisaDiffRWInfo* info, u32 level, u32 offset, u32 size, u32* next_offset, u32* next_size);