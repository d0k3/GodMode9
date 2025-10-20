#pragma once

#include "common.h"
#include "exefs.h"
#include "seedsave.h"

#define NCCH_MEDIA_UNIT 0x200

#define NCCH_EXTHDR_SIZE 0x800 // NCCH header says 0x400, which is not the full thing
#define NCCH_EXTHDR_OFFSET 0x200

#define NCCH_ENCRYPTED(ncch) (!((ncch)->flags[7] & 0x04))
#define NCCH_IS_CXI(ncch) ((ncch)->flags[5] & 0x02)
#define NCCH_IS_FIRM(ncch) (((ncch)->programId >> 32) == 0x00040138)
#define NCCH_IS_GBAVC(exhdr) (getle32((exhdr)->aci_data + 8) == 0x00000202)

#define NCCH_NOCRYPTO  0x0004
#define NCCH_STDCRYPTO 0x0000
#define NCCH_GET_CRYPTO(ncch) (!NCCH_ENCRYPTED(ncch) ? NCCH_NOCRYPTO : (((ncch)->flags[3] << 8) | ((ncch)->flags[7]&(0x01|0x20))))

// wrapper defines
#define DecryptNcch(data, offset, size, ncch, exefs) CryptNcch(data, offset, size, ncch, exefs, NCCH_NOCRYPTO)
#define EncryptNcch(data, offset, size, ncch, exefs, crypto) CryptNcch(data, offset, size, ncch, exefs, crypto)
#define DecryptNcchSequential(data, offset, size) CryptNcchSequential(data, offset, size, NCCH_NOCRYPTO)
#define EncryptNcchSequential(data, offset, size, crypto) CryptNcchSequential(data, offset, size, crypto)

// see: https://www.3dbrew.org/wiki/NCCH/Extended_Header
// very limited, contains only required stuff
typedef struct {
    // SCI (system control info)
    char name[8];
    u8  reserved0[0x5];
    u8  flag; // bit 1 for SD, bit 0 for compressed .code
    u16 remaster_version;
    u8  sci_data[0x30];
    u8  dependencies[0x180];
    u64 savedata_size;
    u64 jump_id;
    u8  reserved1[0x30];
    // ACI (access control info)
    u64 aci_title_id;
    u32 aci_core_version;
    u8  aci_data[0x200 - 0xC];
    // signature and key
    u8  signature[0x100];
    u8  public_key[0x100];
    // limitACI (access control info)
    u64 aci_limit_title_id;
    u32 aci_limit_core_version;
    u8  aci_limit_data[0x200 - 0xC];
} __attribute__((packed, aligned(16))) NcchExtHeader;

// see: https://www.3dbrew.org/wiki/NCCH#NCCH_Header
typedef struct {
    u8  signature[0x100];
    u8  magic[0x4];
    u32 size;
    u64 partitionId;
    u16 makercode;
    u16 version;
    u32 hash_seed;
    u64 programId;
    u8  reserved0[0x10];
    u8  hash_logo[0x20];
    char productcode[0x10];
    u8  hash_exthdr[0x20];
    u32 size_exthdr;
    u8  reserved1[0x4];
    u8  flags[0x8];
    u32 offset_plain;
    u32 size_plain;
    u32 offset_logo;
    u32 size_logo;
    u32 offset_exefs;
    u32 size_exefs;
    u32 size_exefs_hash;
    u8  reserved2[0x4];
    u32 offset_romfs;
    u32 size_romfs;
    u32 size_romfs_hash;
    u8  reserved3[0x4];
    u8  hash_exefs[0x20];
    u8  hash_romfs[0x20];
} __attribute__((packed, aligned(16))) NcchHeader;

u32 ValidateNcchHeader(NcchHeader* header);
u32 ValidateNcchSignature(NcchHeader* header, NcchExtHeader* exthdr);
u32 SetNcchKey(NcchHeader* ncch, u16 crypto, u32 keyid);
u32 SetupNcchCrypto(NcchHeader* ncch, u16 crypt_to);
u32 CryptNcch(void* data, u32 offset, u32 size, NcchHeader* ncch, ExeFsHeader* exefs, u16 crypto);
u32 CryptNcchSequential(void* data, u32 offset, u32 size, u16 crypto);
u32 SetNcchSdFlag(void* data);
u32 SetupSystemForNcch(NcchHeader* ncch, bool to_emunand);
