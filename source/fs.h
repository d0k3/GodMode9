#pragma once

#include "common.h"

typedef enum {
    T_NAND_BASE, // might not be needed
    T_NONFAT_ROOT, // might not be needed
    T_FAT_ROOT,
    T_FAT_FILE,
    T_FAT_DIR
} EntryType;

#define MAX_ENTRIES 1024

typedef struct {
    char* name; // should point to the correct portion of the path
    char path[256];
    u32 size;
    EntryType type;
} DirEntry;

typedef struct {
    u32 n_entries;
    DirEntry entry[MAX_ENTRIES];
} DirStruct;

bool InitFS();
void DeinitFS();

/** Check if file exists **/
bool FileExists(const char* path);

/** Create / overwrite file and write the provided data to it **/
bool FileCreate(const char* path, u8* data, u32 size);

/** Create a screenshot of the current framebuffer **/
void Screenshot();

/** Get directory content under a given path **/
DirStruct* GetDirContents(const char* path);

/** Gets remaining space on SD card in bytes */
uint64_t RemainingStorageSpace();

/** Gets total space on SD card in bytes */
uint64_t TotalStorageSpace();
