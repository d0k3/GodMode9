#pragma once

#include "common.h"

typedef enum {
    T_FAT_ROOT,
    T_FAT_FILE,
    T_FAT_DIR
} EntryType;

#define MAX_ENTRIES 1024

typedef struct {
    char* name; // should point to the correct portion of the path
    char path[256];
    u64 size;
    EntryType type;
    u8 marked;
} DirEntry;

typedef struct {
    u32 n_entries;
    DirEntry entry[MAX_ENTRIES];
} DirStruct;

bool InitFS();
void DeinitFS();

/** Create / overwrite file and write the provided data to it **/
bool FileCreate(const char* path, u8* data, u32 size);

/** Recursively copy a file or directory **/
bool PathCopy(const char* destdir, const char* orig);

/** Recursively delete a file or directory **/
bool PathDelete(const char* path);

/** Create a screenshot of the current framebuffer **/
void Screenshot();

/** Get directory content under a given path **/
DirStruct* GetDirContents(const char* path);

/** Gets remaining space in filesystem in bytes */
uint64_t GetFreeSpace(const char* path);

/** Gets total spacein filesystem in bytes */
uint64_t GetTotalSpace(const char* path);
