#pragma once

#include "common.h"
#include "nand.h"

#define VFLAG_EXT_NAND_REMOUNT (1<<31)

static const char* virtualFileList[] = { // must have a match in virtualFileTemplates[]
    "twln.bin", "twlp.bin", "agbsave.bin", "firm0.bin", "firm1.bin", "ctrnand_fat.bin", "ctrnand_full.bin",
    "nand.bin", "nand_minsize.bin", "nand_hdr.bin", "sector0x96.bin"
};
static const u32 virtualFileList_size = sizeof(virtualFileList) / sizeof(char*);

typedef struct {
    const char name[32];
    u32 offset; // must be a multiple of 0x200
    u32 size;
    u32 keyslot;
    u32 flags;
} __attribute__((packed)) VirtualFile;

bool IsVirtualPath(const char* path);
bool FindVirtualFile(VirtualFile* vfile, const char* path);
int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count);
