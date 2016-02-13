#pragma once

#include "common.h"
#include "decryptor/decryptor.h"

#define GC_NCCH_PROCESS (1<<0)
#define GC_CIA_PROCESS  (1<<1)
#define GC_CIA_DEEP     (1<<2)
#define GC_NCCH_ENCRYPT (1<<3)
#define GC_CIA_ENCRYPT  (1<<4)
#define GC_CXI_ONLY     (1<<5)

#define MAX_ENTRIES 1024

typedef struct {
    u64 titleId;
    u8 external_seed[16];
    u8 reserved[8];
} __attribute__((packed)) SeedInfoEntry;

typedef struct {
    u32 n_entries;
    u8 padding[12];
    SeedInfoEntry entries[MAX_ENTRIES];
} __attribute__((packed)) SeedInfo;

typedef struct {
    u8   ctr[16];
    u32  size_mb;
    char filename[180];
} __attribute__((packed)) SdInfoEntry;

typedef struct {
    u32 n_entries;
    SdInfoEntry entries[MAX_ENTRIES];
} __attribute__((packed, aligned(16))) SdInfo;

typedef struct {
    u8   ctr[16];
    u8   keyY[16];
    u32  size_mb;
    u8   reserved[4];
    u32  usesSeedCrypto;
    u32  uses7xCrypto;
    u64  titleId;
    char filename[112];
} __attribute__((packed)) NcchInfoEntry;

typedef struct {
    u32 padding;
    u32 ncch_info_version;
    u32 n_entries;
    u8  reserved[4];
    NcchInfoEntry entries[MAX_ENTRIES];
} __attribute__((packed, aligned(16))) NcchInfo;

typedef struct {
    u8  signature[0x100];
    u8  magic[0x4];
    u32 size;
    u64 partitionId;
    u16 makercode;
    u16 version;
    u8  reserved0[0x4];
    u64 programId;
    u8  reserved1[0x10];
    u8  hash_logo[0x20];
    char productCode[0x10];
    u8  hash_exthdr[0x20];
    u32 size_exthdr;
    u8  reserved2[0x4];
    u8  flags[0x8];
    u32 offset_plain;
    u32 size_plain;
    u32 offset_logo;
    u32 size_logo;
    u32 offset_exefs;
    u32 size_exefs;
    u32 size_exefs_hash;
    u8  reserved3[0x4];
    u32 offset_romfs;
    u32 size_romfs;
    u32 size_romfs_hash;
    u8  reserved4[0x4];
    u8  hash_exefs[0x20];
    u8  hash_romfs[0x20];
} __attribute__((packed, aligned(16))) NcchHeader;


u32 GetSdCtr(u8* ctr, const char* path);
u32 GetSd0x34KeyY(u8* movable_keyY, bool from_nand);
u32 SdFolderSelector(char* path, u8* keyY);
u32 SdInfoGen(SdInfo* info, const char* base_path);
u32 CryptSdToSd(const char* filename, u32 offset, u32 size, CryptBufferInfo* info);
u32 GetHashFromFile(const char* filename, u32 offset, u32 size, u8* hash);
u32 CheckHashFromFile(const char* filename, u32 offset, u32 size, u8* hash);
u32 CryptNcch(const char* filename, u32 offset, u32 size, u64 seedId, u8* encrypt_flags);
u32 CryptCia(const char* filename, u8* ncch_crypt, bool cia_encrypt, bool cxi_only);

// --> FEATURE FUNCTIONS <--
u32 NcchPadgen(u32 param);
u32 SdPadgen(u32 param);
u32 SdPadgenDirect(u32 param);
u32 UpdateSeedDb(u32 param);
u32 CryptGameFiles(u32 param);
u32 CryptSdFiles(u32 param);
u32 DecryptSdFilesDirect(u32 param);
