#pragma once

#include "common.h"
#include "nand.h"

#define AGBSAVE_MAGIC   '.', 'S', 'A', 'V'

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
    u8  savegame[(0x000180-1)*0x200]; // unknown on custom partitions
} __attribute__((packed)) AgbSave;

u32 GetAgbSaveSize(u32 nand_src);
u32 CheckAgbSaveCmac(u32 nand_src);
u32 FixAgbSaveCmac(u32 nand_dst);
