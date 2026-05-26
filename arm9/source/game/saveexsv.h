#pragma once

#include "types.h"
#include "ifat_common.h"

// SAVE images may be created in two modes:
// - "duplicate data" mode, in which both filesystem metadata and the actual data FAT are duplicated
//   (partitionA: file metadata and FAT, duplicated by DPFS tree)
// - "duplicate meta" mode, in which only the filesystem metadata is duplicated, and the FAT is not
//   (partitionA: file metadata, duplicated by DPFS tree. partitionB: data FAT)

typedef enum ExsvAction
{
    EXSV_ACTION_NONE        = 0x0, // no last action
    EXSV_ACTION_DELETE_FILE = 0x1, // last action was a file deletion
    EXSV_ACTION_CREATE_FILE = 0x2, // last action was a file creation
} ExsvAction;

typedef struct __attribute__((packed)) ExsvExtraHeader {
    u64 sub_file_base_id;
    u32 pending_action;
    u32 unk2;
    u64 last_file_id;
    char LastFilePath[256];
} ExsvExtraHeader;

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
    union {
        u64 file_size;      // for SAVE
        u64 file_unique_id; // for EXSV
    };
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

typedef struct SaveExsvFile {
    ExsvExtraHeader exsv_extra_hdr;
    IFatFsInfo fs_info;
    IFatPreHeader pre_header;
    u32 *dir_hashtbl;
    u32 *file_hashtbl;
    IFatEntry *fat_entries;
    SaveDirectoryEntry *dir_entries;
    u32 max_num_dir_entries;
    SaveFileEntry *file_entries;
    u32 max_num_file_entries;
    bool duplicate_meta;
    bool is_exsv;
    bool init_ok;
} SaveExsvFile;

int SaveExsvFileInit(SaveExsvFile *sav);
int SaveExsvReadFatFile(SaveExsvFile *sav, u32 index, void *buffer, u32 offset, u32 count);
void SaveExsvFileFree(SaveExsvFile *sav);