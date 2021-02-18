#pragma once

#include "common.h"
#include "disadiff.h"

#define SEEDINFO_NAME         "seeddb.bin"
#define SEEDINFO_SIZE(sdb)    (16 + ((sdb)->n_entries * sizeof(SeedInfoEntry)))

#define SEEDSAVE_MAX_ENTRIES  2000
#define SEEDSAVE_AREA_OFFSET  0x3000

typedef struct {
    u8 byte[16];
} PACKED_STRUCT Seed;

typedef struct {
    u64 titleId;
    Seed seed;
    u8 reserved[8];
} PACKED_STRUCT SeedInfoEntry;

typedef struct {
    u32 n_entries;
    u8 padding[12];
    SeedInfoEntry entries[SEEDSAVE_MAX_ENTRIES]; // this number is only a placeholder
} PACKED_STRUCT SeedInfo;

typedef struct {
    u32 unknown0;
    u32 n_entries;
    u8  unknown1[0x1000 - 0x8];
    u64 titleId[SEEDSAVE_MAX_ENTRIES];
    Seed seed[SEEDSAVE_MAX_ENTRIES];
} PACKED_STRUCT SeedDb;

u32 GetSeedPath(char* path, const char* drv);
u32 FindSeed(u8* seed, u64 titleId, u32 hash_seed);
u32 AddSeedToDb(SeedInfo* seed_info, SeedInfoEntry* seed_entry);
u32 InstallSeedDbToSystem(SeedInfo* seed_info, bool to_emunand);
u32 SetupSeedPrePurchase(u64 titleId, bool to_emunand);
u32 SetupSeedSystemCrypto(u64 titleId, u32 hash_seed, bool to_emunand);
