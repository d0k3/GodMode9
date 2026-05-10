#pragma once

#include "types.h"

// SAVE images may be created in two modes:
// - "duplicate data" mode, in which both filesystem metadata and the actual data FAT are duplicated
//   (partitionA: file metadata and FAT, duplicated by DPFS tree)
// - "duplicate meta" mode, in which only the filesystem metadata is duplicated, and the FAT is not
//   (partitionA: file metadata, duplicated by DPFS tree. partitionB: data FAT)

// for dupdata, this describes data inside the data FAT
typedef struct __attribute__((packed)) SaveTableInfoDataRegion {
	u32 starting_block_index;
	u32 block_count;
	u32 max_entry_count;
	u32 _pad;
} SaveTableInfoDataRegion;

// for dupmeta, this describes data outside the FAT
typedef struct __attribute__((packed)) SaveTableInfoStandalone {
	u64 offset;
	u32 count;
	u32 _pad;
} SaveTableInfoStandalone;

typedef union SaveTableInfo {
	SaveTableInfoDataRegion dupdata;
	SaveTableInfoStandalone dupmeta;
} SaveTableInfo;

/* starting block index -> rel to data region */
/* everything else -> rel to header start */
typedef struct __attribute__((packed)) SaveHeader {
	u32 magic; /* 0x0-0x4 */
	u32 version; /* 0x4-0x8 */
	u64 fs_info_offset; /* 0x8-0x10 */
	u64 fs_image_size_blocks; /* 0x10-0x18 */
	u32 fs_image_blocksize; /* 0x18-0x1C */
	u32 __pad; /* 0x1C-0x20 */
	u32 _unk; /* 0x20-0x24 */
	u32 data_region_blocksize; /* 0x24-0x28 */
	SaveTableInfoStandalone dir_hashtbl;
	SaveTableInfoStandalone file_hashtbl;
	SaveTableInfoStandalone fat;
	SaveTableInfoStandalone data_region;
	SaveTableInfo dirtable_info; /* 0x68-0x78 */
	SaveTableInfo filetable_info; /* 0x78-0x88 */
} SaveHeader;

typedef struct __attribute__((packed)) SaveFatEntryHalf {
	u32 index: 31;
	u32 flag: 1;
} SaveFatEntryHalf;

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
typedef struct __attribute__((packed)) SaveFatEntry {
	SaveFatEntryHalf U, V;
} SaveFatEntry;

// same for files and directories
typedef struct __attribute__((packed)) SaveEntryKey {
	u32 parent_dir_idx;
	char name[16];
} SaveEntryKey;

typedef struct SaveDirectoryEntryData {
	SaveEntryKey key;
	u32 next_sibling_dir_index;
	u32 first_subdir_index;
	u32 first_file_index; /* in file entry table */
	u32 __pad;
	u32 next_dir_in_bucket_index;
} SaveDirectoryEntryData;

typedef struct SaveDummyDirectoryEntryData {
	u32 current_total_entry_count;
	u32 max_entry_count; /* max dir count + 2 */
	u8 __pad[0x1C];
	u32 next_dummy_index;
} SaveDummyDirectoryEntryData;

typedef union __attribute__((packed)) SaveDirectoryEntry {
	SaveDummyDirectoryEntryData dmy;
	SaveDirectoryEntryData ent;
} SaveDirectoryEntry;

typedef struct __attribute__((packed)) SaveFileEntryData {
	SaveEntryKey key;
	u32 next_sibling_index;
	u32 __pad;
	u32 first_block_index; /* in data region; 0x80000000 if no data */
	u64 file_size;
	u32 _unk_pad;
	u32 next_file_in_bucket_index;
} SaveFileEntryData;

typedef struct SaveDummyFileEntryData {
	u32 current_total_entry_count;
	u32 max_entry_count; /* max file count + 1 */
	u8 __pad[0x24];
	u32 next_dummy_index;
} SaveDummyFileEntryData;

typedef union SaveFileEntry {
	SaveDummyFileEntryData dmy;
	SaveFileEntryData ent;
} SaveFileEntry;

typedef struct SaveFile {
    SaveHeader header;
	u32 *dir_hashtbl;
	u32 *file_hashtbl;
	SaveFatEntry *fat_entries;
	SaveDirectoryEntry *dir_entries;
	u32 max_num_dir_entries;
	SaveFileEntry *file_entries;
	u32 max_num_file_entries;
	u8 *block_buffer;
	bool duplicate_meta;
	bool init_ok;
} SaveFile;

int SaveFileInit(SaveFile *sav);
int SaveReadFile(SaveFile *sav, u32 index, void *buffer, u32 offset, u32 count);
void SaveFileFree(SaveFile *sav);