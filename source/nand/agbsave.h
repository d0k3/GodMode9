#pragma once

#include "common.h"
#include "nand.h"

#define AGBSAVE_MAGIC       '.', 'S', 'A', 'V'
#define AGBSAVE_MAX_SIZE    (0x000180 * 0x200) // standard size of the NAND partition
#define AGBSAVE_MAX_SSIZE   (AGBSAVE_MAX_SIZE - sizeof(AgbSaveHeader))

// see: http://3dbrew.org/wiki/3DS_Virtual_Console#NAND_Savegame
typedef struct {
	u8  magic[4]; // ".SAV"
    u8  reserved0[0xC]; // always 0xFF
    u8  cmac[0x10];
    u8  reserved1[0x10]; // always 0xFF
    u32 unknown0; // always 0x01
    u32 times_saved;
    u64 title_id;
    u8  sd_cid[0x10];
    u32 save_start; // always 0x200
    u32 save_size;
    u8  reserved2[0x8]; // always 0xFF
    u32 unknown1; // has to do with ARM7?
    u32 unknown2; // has to do with ARM7?
    u8  reserved3[0x198]; // always 0xFF
} __attribute__((packed)) AgbSaveHeader;

u32 ValidateAgbSaveHeader(AgbSaveHeader* header);
u32 GetAgbSaveSize(u32 nand_src);
u32 CheckAgbSaveCmac(u32 nand_src);
u32 FixAgbSaveCmac(u32 nand_dst);
