#include "seedsave.h"
#include "support.h"
#include "nandcmac.h"
#include "sha.h"
#include "ff.h"

#define TITLETAG_MAX_ENTRIES  2000 // same as SEEDSAVE_MAX_ENTRIES
#define TITLETAG_AREA_OFFSET  0x10000 // thanks @luigoalma

// this structure is 0x80 bytes, thanks @luigoalma
typedef struct {
    char magic[4]; // "PREP" for prepurchase install. NIM excepts "PREP" to do seed downloads on the background.
    // playable date parameters
    // 2000-01-01 is a safe bet for a stub entry
    s32 year;
    u8 month;
    u8 day;
    u16 country_code; // enum list of values, this will affect seed downloading, just requires at least one valid enum value. 1 == Japan, it's enough.
    // everything after this point can be 0 padded
    u32 seed_status; // 0 == not tried, 1 == last attempt failed, 2 == seed downloaded successfully
    s32 seed_result; // result related to last download attempt
    s32 seed_support_error_code; // support code derived from the result code 
    // after this point, all is unused or padding. NIM wont use or access this at all.
    // It's memset to 0 by NIM
    u8 unknown[0x68];
} PACKED_STRUCT TitleTagEntry;

typedef struct {
    u32 unknown0;
    u32 n_entries;
    u8  unknown1[0x1000 - 0x8];
    u64 titleId[TITLETAG_MAX_ENTRIES];
    TitleTagEntry tag[TITLETAG_MAX_ENTRIES];
} PACKED_STRUCT TitleTag;

u32 GetSeedPath(char* path, const char* drv) {
    u8 movable_keyy[16] = { 0 };
    u32 sha256sum[8];
    UINT btr = 0;
    FIL file;

    // grab the key Y from movable.sed
    // wrong result if movable.sed does not have it
    snprintf(path, 128, "%2.2s/private/movable.sed", drv);
    if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    f_lseek(&file, 0x110);
    f_read(&file, movable_keyy, 0x10, &btr);
    f_close(&file);
    if (btr != 0x10)
        return 1;

    // build the seed save path
    sha_quick(sha256sum, movable_keyy, 0x10, SHA256_MODE);
    snprintf(path, 128, "%2.2s/data/%08lX%08lX%08lX%08lX/sysdata/0001000F/00000000",
        drv, sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);

    return 0;
}

u32 FindSeed(u8* seed, u64 titleId, u32 hash_seed) {
    static u8 lseed[16+8] __attribute__((aligned(4))) = { 0 }; // seed plus title ID for easy validation
    u32 sha256sum[8];

    memcpy(lseed+16, &titleId, 8);
    sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
    if (hash_seed == sha256sum[0]) {
        memcpy(seed, lseed, 16);
        return 0;
    }

    // setup a large enough buffer
    u8* buffer = (u8*) malloc(max(STD_BUFFER_SIZE, sizeof(SeedDb)));
    if (!buffer) return 1;
    
    // try to grab the seed from NAND database
    const char* nand_drv[] = {"1:", "4:"}; // SysNAND and EmuNAND
    for (u32 i = 0; i < countof(nand_drv); i++) {
        char path[128];
        SeedDb* seeddb = (SeedDb*) (void*) buffer;

        // read SEEDDB from file
        if (GetSeedPath(path, nand_drv[i]) != 0) continue;
        if ((ReadDisaDiffIvfcLvl4(path, NULL, SEEDSAVE_AREA_OFFSET, sizeof(SeedDb), seeddb) != sizeof(SeedDb)) ||
            (seeddb->n_entries > SEEDSAVE_MAX_ENTRIES))
            continue;

        // search for the seed
        for (u32 s = 0; s < seeddb->n_entries; s++) {
            if (titleId != seeddb->titleId[s]) continue;
            memcpy(lseed, &(seeddb->seed[s]), sizeof(Seed));
            sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
            if (hash_seed == sha256sum[0]) {
                memcpy(seed, lseed, 16);
                free(buffer);
                return 0; // found!
            }
        }
    }
    
    // not found -> try seeddb.bin
    SeedInfo* seeddb = (SeedInfo*) (void*) buffer;
    size_t len = LoadSupportFile(SEEDINFO_NAME, seeddb, STD_BUFFER_SIZE);
    if (len && (seeddb->n_entries <= (len - 16) / 32)) { // check filesize / seeddb size
        for (u32 s = 0; s < seeddb->n_entries; s++) {
            if (titleId != seeddb->entries[s].titleId)
                continue;
            memcpy(lseed, &(seeddb->entries[s].seed), sizeof(Seed));
            sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
            if (hash_seed == sha256sum[0]) {
                memcpy(seed, lseed, 16);
                free(buffer);
                return 0; // found!
            }
        }
    }

    // out of options -> failed!
    free(buffer);
    return 1;
}

u32 AddSeedToDb(SeedInfo* seed_info, SeedInfoEntry* seed_entry) {
    if (!seed_entry) { // no seed entry -> reset database
        memset(seed_info, 0, 16);
        return 0;
    }
    // check if entry already in DB
    u32 n_entries = seed_info->n_entries;
    SeedInfoEntry* seed = seed_info->entries;
    for (u32 i = 0; i < n_entries; i++, seed++)
        if (seed->titleId == seed_entry->titleId) return 0;
    // actually a new seed entry
    memcpy(seed, seed_entry, sizeof(SeedInfoEntry));
    seed_info->n_entries++;
    return 0;
}

u32 InstallSeedDbToSystem(SeedInfo* seed_info, bool to_emunand) {
    char path[128];
    SeedDb* seeddb = (SeedDb*) malloc(sizeof(SeedDb));
    if (!seeddb) return 1;

    // read the current SEEDDB database
    if ((GetSeedPath(path, to_emunand ? "4:" : "1:") != 0) ||
        (ReadDisaDiffIvfcLvl4(path, NULL, SEEDSAVE_AREA_OFFSET, sizeof(SeedDb), seeddb) != sizeof(SeedDb)) ||
        (seeddb->n_entries >= SEEDSAVE_MAX_ENTRIES)) {
        free (seeddb);
        return 1;
    }

    // find free slots, insert seeds from SeedInfo
    for (u32 slot = 0, s = 0; s < seed_info->n_entries; s++) {
        SeedInfoEntry* entry = &(seed_info->entries[s]);
        for (slot = 0; slot < seeddb->n_entries; slot++)
            if (seeddb->titleId[slot] == entry->titleId) break;
        if (slot >= SEEDSAVE_MAX_ENTRIES) break;
        if (slot >= seeddb->n_entries) seeddb->n_entries = slot + 1;
        seeddb->titleId[slot] = entry->titleId;
        memcpy(&(seeddb->seed[slot]), &(entry->seed), sizeof(Seed));
    }

    // write back to system (warning: no write protection checks here)
    u32 size = WriteDisaDiffIvfcLvl4(path, NULL, SEEDSAVE_AREA_OFFSET, sizeof(SeedDb), seeddb);
    FixFileCmac(path, false);

    free (seeddb);
    return (size == sizeof(SeedDb)) ? 0 : 1;
}

u32 SetupSeedPrePurchase(u64 titleId, bool to_emunand) {
    // here, we ask the system to install the seed for us
    TitleTag* titletag = (TitleTag*) malloc(sizeof(TitleTag));
    if (!titletag) return 1;
    
    char path[128];
    if ((GetSeedPath(path, to_emunand ? "4:" : "1:") != 0) ||
        (ReadDisaDiffIvfcLvl4(path, NULL, TITLETAG_AREA_OFFSET, sizeof(TitleTag), titletag) != sizeof(TitleTag)) ||
        (titletag->n_entries >= TITLETAG_MAX_ENTRIES)) {
        free (titletag);
        return 1;
    }
    
    // pointers for TITLETAG title IDs and seeds
    // find a free slot, insert titletag
    u32 slot = 0;
    for (; slot < titletag->n_entries; slot++)
        if (titletag->titleId[slot] == titleId) break;
    if (slot >= titletag->n_entries)
        titletag->n_entries = slot + 1;
    
    TitleTagEntry* ttag = &(titletag->tag[slot]);
    titletag->titleId[slot] = titleId;
    memset(ttag, 0, sizeof(TitleTagEntry));
    memcpy(ttag->magic, "PREP", 4);
    ttag->year = 2000;
    ttag->month = 1;
    ttag->day = 1;
    ttag->country_code = 1;

    // write back to system (warning: no write protection checks here)
    u32 size = WriteDisaDiffIvfcLvl4(path, NULL, TITLETAG_AREA_OFFSET, sizeof(TitleTag), titletag);
    FixFileCmac(path, false);
    
    free(titletag);
    return (size == sizeof(TitleTag)) ? 0 : 1;
}

u32 SetupSeedSystemCrypto(u64 titleId, u32 hash_seed, bool to_emunand) {
    // attempt to find the seed inside the seeddb.bin support file
    SeedInfo* seeddb = (SeedInfo*) malloc(STD_BUFFER_SIZE);
    if (!seeddb) return 1;

    size_t len = LoadSupportFile(SEEDINFO_NAME, seeddb, STD_BUFFER_SIZE);
    if (len && (seeddb->n_entries <= (len - 16) / 32)) { // check filesize / seeddb size
        for (u32 s = 0; s < seeddb->n_entries; s++) {
            if (titleId != seeddb->entries[s].titleId)
                continue;
            // found a candidate, hash and verify it
            u8 lseed[16+8] __attribute__((aligned(4))) = { 0 }; // seed plus title ID for easy validation
            u32 sha256sum[8];
            memcpy(lseed+16, &titleId, 8);
            memcpy(lseed, &(seeddb->entries[s].seed), sizeof(Seed));
            sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
            u32 res = 0; // assuming the installed seed to be correct
            if (hash_seed == sha256sum[0]) {
                // found, install it
                seeddb->n_entries = 1;
                seeddb->entries[0].titleId = titleId;
                memcpy(&(seeddb->entries[0].seed), lseed, sizeof(Seed));
                res = InstallSeedDbToSystem(seeddb, to_emunand);
            }
            free(seeddb);
            return res;
        }
    }

    free(seeddb);
    return 1;
}
