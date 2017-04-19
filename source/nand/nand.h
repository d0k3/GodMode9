#pragma once

#include "common.h"

#define NAND_SYSNAND    (1UL<<0)
#define NAND_EMUNAND    (1UL<<1)
#define NAND_IMGNAND    (1UL<<2)
#define NAND_ZERONAND   (1UL<<3)
#define NAND_TYPE_O3DS  (1UL<<4)
#define NAND_TYPE_N3DS  (1UL<<5)
#define NAND_TYPE_NO3DS (1UL<<6)
#define NAND_TYPE_TWL   (1UL<<7)

// minimum size of NAND memory
#define NAND_MIN_SECTORS_O3DS 0x1D7800
#define NAND_MIN_SECTORS_N3DS 0x26C000

// start sectors of partitions
#define SECTOR_TWL      0x000000
#define SECTOR_SECRET   0x000096
#define SECTOR_TWLN     0x000097
#define SECTOR_TWLP     0x04808D
#define SECTOR_AGBSAVE  0x058800
#define SECTOR_FIRM0    0x058980
#define SECTOR_FIRM1    0x05A980
#define SECTOR_CTR      0x05C980

// sizes of partitions (in sectors)
#define SIZE_TWL        0x058800
#define SIZE_TWLN       0x047DA9
#define SIZE_TWLP       0x0105B3
#define SIZE_AGBSAVE    0x000180
#define SIZE_FIRM0      0x002000
#define SIZE_FIRM1      0x002000
#define SIZE_CTR_O3DS   0x17AE80
#define SIZE_CTR_N3DS   0x20F680

// filenames for sector 0x96
#define SECTOR_NAME     "sector0x96.bin"
#define SECRET_NAME     "secret_sector.bin"
#define OTP_NAME        "otp.bin"
#define OTP_BIG_NAME    "otp0x108.bin"  

bool InitNandCrypto(void);
bool CheckSlot0x05Crypto(void);
bool CheckSector0x96Crypto(void);

void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot);
void CryptSector0x96(u8* buffer, bool encrypt);
int ReadNandBytes(u8* buffer, u64 offset, u64 count, u32 keyslot, u32 nand_src);
int WriteNandBytes(const u8* buffer, u64 offset, u64 count, u32 keyslot, u32 nand_dst);
int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, u32 src);
int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 dest);

u64 GetNandSizeSectors(u32 src);
u64 GetNandUnusedSectors(u32 src);
u32 CheckNandMbr(u8* mbr);
u32 CheckNandHeader(u8* header);
u32 CheckNandType(u32 src);

u32 GetLegitSector0x96(u8* sector);
u32 GetOtpHash(void* hash);
u32 GetNandCid(void* cid);

bool CheckMultiEmuNand(void);
u32 InitEmuNandBase(bool reset);
u32 GetEmuNandBase(void);
