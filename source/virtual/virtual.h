#pragma once

#include "common.h"
#include "dir.h"
#include "nand.h"

#define VRT_SYSNAND NAND_SYSNAND
#define VRT_EMUNAND NAND_EMUNAND
#define VRT_IMGNAND NAND_IMGNAND
#define VRT_MEMORY  (1<<10)
#define VRT_GAME    (1<<11)

#define VFLAG_A9LH_AREA (1<<20)

// virtual file flag (subject to change):
// bits 0...9  : reserved for NAND virtual sources and info
// bits 10...15: reserved for other virtual sources
// bits 16...19: reserved for external flags
// bits 20...31: reserved for internal flags (different per source)
typedef struct {
    char name[32];
    u32 offset; // must be a multiple of 0x200 (for NAND access)
    u32 size;
    u32 keyslot;
    u32 flags;
} __attribute__((packed)) VirtualFile;

u32 GetVirtualSource(const char* path);
bool CheckVirtualDrive(const char* path);
bool GetVirtualFile(VirtualFile* vfile, const char* path);
bool FindVirtualFileBySize(VirtualFile* vfile, const char* path, u32 size);
bool GetVirtualDirContents(DirStruct* contents, const char* path, const char* pattern);
int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count, u32* bytes_read);
int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count, u32* bytes_written);
