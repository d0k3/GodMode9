#pragma once

#include "common.h"

#define FIRM_MAGIC  'F', 'I', 'R', 'M'

#define FIRM_MAX_SIZE  0x400000 // 4MB, due to FIRM partition size
#define ARM11NCCH_OFFSET 0, 0x2A000, 0x2B000, 0x2C000
#define ARM9BIN_OFFSET 0x800
// ARM9 entrypoint after decryption - TWL/AGB and NATIVE/SAFE_MODE
// see: https://github.com/AuroraWright/Luma3DS/blob/master/source/firm.c#L349
// and: https://github.com/AuroraWright/Luma3DS/blob/master/source/firm.c#L424
// and: https://github.com/AuroraWright/Luma3DS/blob/master/source/firm.c#L463
#define ARM9ENTRY_FIX(firm) (((firm)->sections[3].offset) ?  0x801301C : 0x0801B01C)

#define FIRM_NDMA_CPY   0
#define FIRM_XDMA_CPY   1
#define FIRM_CPU_MEMCPY 2

#define IsInstallableFirm(firm, firm_size) (ValidateFirm(firm, firm_size, true) == 0)
#define IsBootableFirm(firm, firm_size) (ValidateFirm(firm, firm_size, false) == 0)


// see: https://www.3dbrew.org/wiki/FIRM#Firmware_Section_Headers
typedef struct {
    u32 offset;
    u32 address;
    u32 size;
    u32 method;
    u8  hash[0x20];
} __attribute__((packed)) FirmSectionHeader;

// see: https://www.3dbrew.org/wiki/FIRM#FIRM_Header
typedef struct {
    u8  magic[4];
    u8  dec_magic[4];
    u32 entry_arm11;
    u32 entry_arm9;
    u8  reserved0[0x30];
    FirmSectionHeader sections[4];
    u8  signature[0x100];
} __attribute__((packed, aligned(16))) FirmHeader;

// see: https://www.3dbrew.org/wiki/FIRM#New_3DS_FIRM
typedef struct {
    u8  keyX0x15[0x10]; // this is encrypted
    u8  keyY0x150x16[0x10];
    u8  ctr[0x10];
    char size_ascii[0x8];
    u8  reserved[0x8];
    u8  control[0x10];
    u8  k9l[0x10];
    u8  keyX0x16[0x10]; // this is encrypted
    u8  padding[0x1A0];
} __attribute__((packed, aligned(16))) FirmA9LHeader;

u32 ValidateFirmHeader(FirmHeader* header, u32 data_size);
u32 ValidateFirmA9LHeader(FirmA9LHeader* header);
u32 ValidateFirm(void* firm, u32 firm_size, bool installable);
u32 GetFirmSize(FirmHeader* header);

FirmSectionHeader* FindFirmArm9Section(FirmHeader* firm);
u32 GetArm9BinarySize(FirmA9LHeader* a9l);

u32 SetupArm9BinaryCrypto(FirmA9LHeader* header);
u32 DecryptA9LHeader(FirmA9LHeader* header);
u32 DecryptFirm(void* data, u32 offset, u32 size, FirmHeader* firm, FirmA9LHeader* a9l);
u32 DecryptArm9Binary(void* data, u32 offset, u32 size, FirmA9LHeader* a9l);
u32 DecryptFirmSequential(void* data, u32 offset, u32 size);
u32 DecryptFirmFull(void* data, u32 size);
