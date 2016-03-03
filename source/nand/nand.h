#pragma once

#include "common.h"

bool InitNandCrypto(void);
void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot);

int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, bool read_emunand);
int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, bool write_emunand);

u32 GetEmuNandBase(void);
u32 SwitchEmuNandBase(int start_sector);

