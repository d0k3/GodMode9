#pragma once

#include "common.h"
#include "nand.h"

#define VRT_SYSNAND NAND_SYSNAND
#define VRT_EMUNAND NAND_EMUNAND
#define VRT_IMGNAND NAND_IMGNAND
#define VRT_XORPAD  NAND_ZERONAND
#define VRT_MEMORY  (1UL<<4)
#define VRT_GAME    (1UL<<5)
#define VRT_TICKDB  (1UL<<6)
#define VRT_KEYDB   (1UL<<7)
#define VRT_CART    (1UL<<8)
#define VRT_VRAM    (1UL<<9)

#define VRT_SOURCE  (VRT_SYSNAND|VRT_EMUNAND|VRT_IMGNAND|VRT_XORPAD|VRT_MEMORY|VRT_GAME|VRT_TICKDB|VRT_KEYDB|VRT_CART|VRT_VRAM)

#define VFLAG_DIR       (1UL<<10)
#define VFLAG_ROOT      (1UL<<11)
#define VFLAG_READONLY  (1UL<<12)
#define VFLAG_DELETABLE (1UL<<13)
#define VFLAG_LV3       (1UL<<14)


#define VRT_DRIVES  {'S', VRT_SYSNAND}, {'E', VRT_EMUNAND}, {'I', VRT_IMGNAND}, {'X', VRT_XORPAD }, \
                    {'M', VRT_MEMORY}, {'G', VRT_GAME}, {'K', VRT_KEYDB}, {'T', VRT_TICKDB}, {'C', VRT_CART}, {'V', VRT_VRAM}

// virtual file flag (subject to change):
// bits 0...3  : reserved for NAND virtual sources and info
// bits 4...9  : reserved for other virtual sources
// bits 10...15: reserved for external flags
// bits 16...31: reserved for internal flags (different per source, see vgame.c)
typedef struct {
    char name[32];
    u64 offset; // must be a multiple of 0x200 (for NAND access)
    u64 size;
    u32 keyslot;
    u32 flags;
} __attribute__((packed)) VirtualFile;

// virtual dirs are only relevant for virtual game drives
typedef struct {
    int index;
    u64 offset;
    u64 size;
    u32 flags;
} __attribute__((packed)) VirtualDir;

u32 GetVirtualSource(const char* path);
bool InitVirtualImageDrive(void);
bool CheckVirtualDrive(const char* path);

bool ReadVirtualDir(VirtualFile* vfile, VirtualDir* vdir);
bool OpenVirtualRoot(VirtualDir* vdir, u32 virtual_src);
bool OpenVirtualDir(VirtualDir* vdir, VirtualFile* ventry);

bool GetVirtualFile(VirtualFile* vfile, const char* path);
bool GetVirtualDir(VirtualDir* vdir, const char* path);
bool GetVirtualFilename(char* name, const VirtualFile* vfile, u32 n_chars);

int ReadVirtualFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count, u32* bytes_read);
int WriteVirtualFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count, u32* bytes_written);
int DeleteVirtualFile(const VirtualFile* vfile);

u64 GetVirtualDriveSize(const char* path);
