#pragma once

#include "common.h"
#include "nand.h"

#define VRT_SYSNAND NAND_SYSNAND
#define VRT_EMUNAND NAND_EMUNAND
#define VRT_IMGNAND NAND_IMGNAND
#define VRT_MEMORY  (1<<10)

#define VFLAG_A9LH_AREA (1<<20)

static const char* virtualFileList[] = { // must have a match in virtualFileTemplates[]
    "twln.bin", "twlp.bin", "agbsave.bin", "firm0.bin", "firm1.bin", "ctrnand_fat.bin",
    "ctrnand_full.bin", "nand.bin", "nand_minsize.bin", "nand_hdr.bin", "twlmbr.bin", "sector0x96.bin",
    "itcm.mem", "arm9.mem", "arm9ext.mem", "vram.mem", "dsp.mem", "axiwram.mem",
    "fcram.mem", "fcramext.mem", "dtcm.mem", "bootrom_unp.mem"
};
static const u32 virtualFileList_size = sizeof(virtualFileList) / sizeof(char*);

typedef struct {
    const char name[32];
    u32 offset; // must be a multiple of 0x200
    u32 size;
    u32 keyslot;
    u32 flags;
} __attribute__((packed)) VirtualFile;

u32 GetVirtualSource(const char* path);
bool CheckVirtualDrive(const char* path);
bool FindVirtualFile(VirtualFile* vfile, const char* path, u32 size);
int ReadVirtualFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count, u32* bytes_read);
int WriteVirtualFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count, u32* bytes_written);
