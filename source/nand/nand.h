#pragma once

#include "common.h"

#define NAND_TYPE_UNK   0
#define NAND_TYPE_O3DS  (1<<0)
#define NAND_TYPE_N3DS  (1<<1)
#define NAND_TYPE_NO3DS (1<<2)

bool InitNandCrypto(void);
void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot);

int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, bool read_emunand);
int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, bool write_emunand);

u8 CheckNandType(bool check_emunand);

bool InitEmuNandBase(void);
u32 GetEmuNandBase(void);
u32 SwitchEmuNandBase(int start_sector);

