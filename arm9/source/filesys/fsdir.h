#pragma once

#include "common.h"

#define MAX_DIR_ENTRIES 1024

typedef enum {
    T_ROOT,
    T_DIR,
    T_FILE,
    T_DOTDOT
} EntryType;

typedef struct {
    char* name; // should point to the correct portion of the path
    char path[256];
    u64 size;
    EntryType type;
    u8 marked;
} DirEntry;

typedef struct {
    u32 n_entries;
    DirEntry entry[MAX_DIR_ENTRIES];
} DirStruct;

void DirEntryCpy(DirEntry* dest, const DirEntry* orig);
void SortDirStruct(DirStruct* contents);
