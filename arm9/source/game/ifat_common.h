#pragma once

#include "types.h"

// These structures are shared across 3 different inner-FAT (named ifat for brevity) types:
// - SAVE: save data
// - EXSV: extdata
// - IRDB (BDRI): title.db & ticket.db

// Each ifat type has its own type of file and directory entry.

typedef struct {
    union {
        // this describes data inside the FAT (and thus, in the data region)
        struct {
            u32 starting_block_index;
            u32 block_count;
            u32 max_entry_count;
        } PACKED_STRUCT;

        // this describes data outside the FAT (and thus, outside the data region)
        struct {
            u64 outfat_offset;
            u32 outfat_count;
        } PACKED_STRUCT;
    };
    u32 _pad;
} PACKED_STRUCT IFatTableInfo;

typedef struct {
    union {
        u32 magic;
        char magic_str[4];
    };
    u32 version;
    u64 fs_info_offset;
    u64 fs_image_size_blocks;
    u32 fs_image_blocksize;
    u32 __pad;
} PACKED_STRUCT IFatPreHeader;

typedef struct {
    u32 _unk;
    u32 data_region_blocksize;
    IFatTableInfo dir_hashtbl;
    IFatTableInfo file_hashtbl;
    IFatTableInfo fat;
    IFatTableInfo data_region;
    IFatTableInfo dirtable_info;
    IFatTableInfo filetable_info;
} PACKED_STRUCT IFatFsInfo;


/*
 * for node head,
 * - U.index --> index of previous node head.
 * - U.flag --> set if this is the first node head.
 * - V.index --> index of next node head.
 * - V.flag --> whether or not this node has extended entries (multiple entries)
 */
/*
 * for extended node,
 * - U.index --> index of previous entry in this node.
 * - U.flag --> always set.
 * - V.index --> index of next entry in this node.
 * - V.flag -->  never set.
 */

typedef struct __attribute__((packed)) IFatEntryHalf {
    u32 index: 31;
    u32 flag: 1;
} IFatEntryHalf;

typedef struct __attribute__((packed)) IFatEntry {
    IFatEntryHalf U, V;
} IFatEntry;