#pragma once

#include "common.h"

#define NAND_SYSNAND    (1<<0)
#define NAND_EMUNAND    (1<<1)
#define NAND_IMGNAND    (1<<2)
#define NAND_TYPE_O3DS  (1<<3)
#define NAND_TYPE_N3DS  (1<<4)
#define NAND_TYPE_NO3DS (1<<5)

bool LoadKeyFromFile(const char* folder, u8* keydata, u32 keyslot, char type, char* id);
bool InitNandCrypto(void);
bool CheckSlot0x05Crypto(void);
bool CheckSector0x96Crypto(void);
bool CheckA9lh(void);

void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot);
void CryptSector0x96(u8* buffer, bool encrypt);
int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, u32 src);
int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 dest);

u64 GetNandSizeSectors(u32 src);
u32 CheckNandType(u32 src);

bool InitEmuNandBase(void);
