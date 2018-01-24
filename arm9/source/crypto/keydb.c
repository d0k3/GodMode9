#include "keydb.h"
#include "aes.h"
#include "sha.h"
#include "vff.h"
#include "support.h"

typedef struct {
    u8   slot;           // keyslot, 0x00...0x39
    u8   keyUnitType;    // 0 for ALL units / 1 for devkit exclusive / 2 for retail exclusive
    u8   sample[16];     // sample data, encoded with src = keyY = ctr = { 0 }
} __attribute__((packed)) AesNcchSampleInfo;

static u64 keyState  = 0;
static u64 keyXState = 0;
static u64 keyYState = 0;

u32 GetUnitKeysType(void)
{
    static u32 keys_type = KEYS_UNKNOWN;
    
    if (keys_type == KEYS_UNKNOWN) {
        static const u8 slot0x2CSampleRetail[16] = {
            0xBC, 0xC4, 0x16, 0x2C, 0x2A, 0x06, 0x91, 0xEE, 0x47, 0x18, 0x86, 0xB8, 0xEB, 0x2F, 0xB5, 0x48 };
        static const u8 slot0x2CSampleDevkit[16] = {
            0x29, 0xB5, 0x5D, 0x9F, 0x61, 0xAC, 0xD2, 0x28, 0x22, 0x23, 0xFB, 0x57, 0xDD, 0x50, 0x8A, 0xF5 };
        static u8 zeroes[16] = { 0 };
        u8 sample[16] = { 0 };
        setup_aeskeyY(0x2C, zeroes);
        use_aeskey(0x2C);
        set_ctr(zeroes);
        aes_decrypt(sample, sample, 1, AES_CNT_CTRNAND_MODE);
        if (memcmp(sample, slot0x2CSampleRetail, 16) == 0) {
            keys_type = KEYS_RETAIL;
        } else if (memcmp(sample, slot0x2CSampleDevkit, 16) == 0) {
            keys_type = KEYS_DEVKIT;
        }
    }
        
    return keys_type;
}

void CryptAesKeyInfo(AesKeyInfo* info) {
    static u8 zeroes[16] = { 0 };
    u8 ctr[16] = { 0 };
    memcpy(ctr, (void*) info, 12); // CTR -> slot + type + id + zeroes
    setup_aeskeyY(0x2C, zeroes);
    use_aeskey(0x2C);
    set_ctr(ctr);
    aes_decrypt(info->key, info->key, 1, AES_CNT_CTRNAND_MODE);
    info->isEncrypted = !info->isEncrypted;
}

u32 CheckKeySlot(u32 keyslot, char type)
{
    static const AesNcchSampleInfo keyNcchSamples[] = {
        { 0x18, KEYS_RETAIL, // Retail NCCH Secure3
         { 0x78, 0xBB, 0x84, 0xFA, 0xB3, 0xA2, 0x49, 0x83, 0x9E, 0x4F, 0x50, 0x7B, 0x17, 0xA0, 0xDA, 0x23 } },
        { 0x1B, KEYS_RETAIL, // Retail NCCH Secure4
         { 0xF3, 0x6F, 0x84, 0x7E, 0x59, 0x43, 0x6E, 0xD5, 0xA0, 0x40, 0x4C, 0x71, 0x19, 0xED, 0xF7, 0x0A } },
        { 0x25, KEYS_RETAIL, // Retail NCCH 7x
         { 0x34, 0x7D, 0x07, 0x48, 0xAE, 0x5D, 0xFB, 0xB0, 0xF5, 0x86, 0xD6, 0xB5, 0x14, 0x65, 0xF1, 0xFF } },
        { 0x18, KEYS_DEVKIT, // DevKit NCCH Secure3
         { 0x20, 0x8B, 0xB5, 0x61, 0x94, 0x18, 0x6A, 0x84, 0x91, 0xD6, 0x37, 0x27, 0x91, 0xF3, 0x53, 0xC9 } },
        { 0x1B, KEYS_DEVKIT, // DevKit NCCH Secure4
         { 0xB3, 0x9D, 0xC1, 0xDB, 0x5B, 0x0C, 0xE7, 0x60, 0xBE, 0xAD, 0x5A, 0xBF, 0xD0, 0x86, 0x99, 0x88 } },
        { 0x25, KEYS_DEVKIT, // DevKit NCCH 7x
         { 0xBC, 0x83, 0x7C, 0xC9, 0x99, 0xC8, 0x80, 0x9E, 0x8A, 0xDE, 0x4A, 0xFA, 0xAA, 0x72, 0x08, 0x28 } }
    };
    u64* state = (type == 'X') ? &keyXState : (type == 'Y') ? &keyYState : (type == 'N') ? &keyState : NULL;
    
    // just to be safe...
    if (keyslot >= 0x40)
        return 1;
    
    // check if key is already loaded
    if ((type != 'I') && ((*state >> keyslot) & 1))
        return 0;
    
    // if is not, we may still be able to verify the currently set one (for NCCH keys)
    for (u32 p = 0; (type == 'X') && (p < sizeof(keyNcchSamples) / sizeof(AesNcchSampleInfo)); p++) {
        if (keyNcchSamples[p].slot != keyslot) // only for keyslots in the keyNcchSamples table!
            continue;
        if (keyNcchSamples[p].keyUnitType && (keyNcchSamples[p].keyUnitType != GetUnitKeysType()))
            continue;
        u8 zeroes[16] = { 0 };
        u8 sample[16] = { 0 };
        setup_aeskeyY(keyslot, zeroes);
        use_aeskey(keyslot);
        set_ctr(zeroes);
        aes_decrypt(sample, sample, 1, AES_CNT_CTRNAND_MODE);
        if (memcmp(keyNcchSamples[p].sample, sample, 16) == 0) {
            keyXState |= 1ull << keyslot;
            return 0;
        }
    }
    
    // not set up if getting here
    return 1;
}

// this function creates dependencies to "vff.h" and "support.h"
// to get rid of these, you may replace this function with anything that works for you
u32 LoadKeyDb(const char* path_db, AesKeyInfo* keydb, u32 bsize) {
    UINT fsize = 0;
    
    if (path_db) {
        if (fvx_qread (path_db, keydb, 0, bsize, &fsize) != FR_OK) fsize = 0;
    } else if (fvx_qread ("S:/" KEYDB_NAME, keydb, 0, bsize, &fsize) != FR_OK) {
        fsize = LoadSupportFile(KEYDB_NAME, keydb, bsize); // load key database support file
    }
    
    u32 nkeys = 0;
    if (fsize && !(fsize % sizeof(AesKeyInfo)))
        nkeys = fsize / sizeof(AesKeyInfo);
    return nkeys;
}

u32 LoadKeyFromFile(void* key, u32 keyslot, char type, char* id)
{
    u8 keystore[16] __attribute__((aligned(32))) = {0};
    bool found = false;
    
    // checking the obvious
    if ((keyslot >= 0x40) || ((type != 'X') && (type != 'Y') && (type != 'N') && (type != 'I')))
        return 1;
    
    // check if already loaded
    if (!key && !id && (CheckKeySlot(keyslot, type) == 0)) return 0;
    
    // use keystore if key == NULL
    if (!key) key = keystore;
    
    // try to get key from 'aeskeydb.bin' file
    AesKeyInfo* keydb = (AesKeyInfo*) malloc(STD_BUFFER_SIZE);
    u32 nkeys = (keydb) ? LoadKeyDb(NULL, keydb, STD_BUFFER_SIZE) : 0;
    
    for (u32 i = 0; i < nkeys; i++) {
        AesKeyInfo* info = &(keydb[i]);
        if (!((info->slot == keyslot) && (info->type == type) && 
            ((!id && !(info->id[0])) || (id && (strncmp(id, info->id, 10) == 0))) &&
            (!info->keyUnitType || (info->keyUnitType == GetUnitKeysType()))))
            continue;
        found = true;
        if (info->isEncrypted)
            CryptAesKeyInfo(info);
        memcpy(key, info->key, 16);
        break;
    }
    
    free(keydb);
    
    // load legacy slot0x??Key?.bin file instead
    if (!found && (type != 'I')) {
        char fname[64];
        snprintf(fname, 64, "slot0x%02lXKey%s%s.bin", keyslot,
            (type == 'X') ? "X" : (type == 'Y') ? "Y" : (type == 'I') ? "IV" : "", (id) ? id : "");
        found = (LoadSupportFile(fname, key, 16) == 16);
    }
    
    // key still not found (duh)
    if (!found) return 1; // out of options here
    
    // done if this is an IV
    if (type == 'I') return 0;
    
    // now, setup the key
    if (type == 'X') {
        setup_aeskeyX(keyslot, key);
        keyXState |= 1ull << keyslot;
    } else if (type == 'Y') {
        setup_aeskeyY(keyslot, key);
        keyYState |= 1ull << keyslot;
    } else { // normalKey includes keyX & keyY
        setup_aeskey(keyslot, key);
        keyState  |= 1ull << keyslot;
        keyXState |= 1ull << keyslot;
        keyYState |= 1ull << keyslot;
    }
    use_aeskey(keyslot);
    
    return 0;
}

u32 InitKeyDb(const char* path)
{
    // use this to quickly initialize all applicable keys in aeskeydb.bin
    static const u64 keyslot_whitelist = (1ull<<0x02)|(1ull<<0x03)|(1ull<<0x05)|(1ull<<0x18)|(1ull<<0x19)|(1ull<<0x1A)|(1ull<<0x1B)|
        (1ull<<0x1C)|(1ull<<0x1D)|(1ull<<0x1E)|(1ull<<0x1F)|(1ull<<0x24)|(1ull<<0x25)|(1ull<<0x2F);
    
    // try to load aeskeydb.bin file
    AesKeyInfo* keydb = (AesKeyInfo*) malloc(STD_BUFFER_SIZE);
    u32 nkeys = (keydb) ? LoadKeyDb(path, keydb, STD_BUFFER_SIZE) : 0;
    
    // apply all applicable keys
    for (u32 i = 0; i < nkeys; i++) {
        AesKeyInfo* info = &(keydb[i]);
        if ((info->slot >= 0x40) || ((info->type != 'X') && (info->type != 'Y') && (info->type != 'N') && (info->type != 'I'))) {
            free(keydb);
            return 1; // looks faulty, better stop right here
        }
        if (!path && !((1ull<<info->slot)&keyslot_whitelist)) continue; // not in keyslot whitelist
        if ((info->type == 'I') || (*(info->id)) || (info->keyUnitType && (info->keyUnitType != GetUnitKeysType())) ||
            (CheckKeySlot(info->slot, info->type) == 0)) continue; // most likely valid, but not applicable or already set
        if (info->isEncrypted) CryptAesKeyInfo(info); // decrypt key
        
        // apply key
        u8 key[16] __attribute__((aligned(32))) = {0};
        char type = info->type;
        u32 keyslot = info->slot;
        memcpy(key, info->key, 16);
        if (type == 'X') {
            setup_aeskeyX(keyslot, key);
            keyXState |= 1ull << keyslot;
        } else if (type == 'Y') {
            setup_aeskeyY(keyslot, key);
            keyYState |= 1ull << keyslot;
        } else { // normalKey includes keyX & keyY
            setup_aeskey(keyslot, key);
            keyState  |= 1ull << keyslot;
            keyXState |= 1ull << keyslot;
            keyYState |= 1ull << keyslot;
        }
        use_aeskey(keyslot);
    }
    
    free(keydb);
    return (nkeys) ? 0 : 1;
}

// creates dependency to "sha.h", not required for base keydb functions
u32 CheckRecommendedKeyDb(const char* path)
{
    // SHA-256 of the recommended aeskeydb.bin file
    // equals MD5 A5B28945A7C051D7A0CD18AF0E580D1B
    const u8 recommended_sha[0x20] = {
        0x40, 0x76, 0x54, 0x3D, 0xA3, 0xFF, 0x91, 0x1C, 0xE1, 0xCC, 0x4E, 0xC7, 0x2F, 0x92, 0xE4, 0xB7,
        0x2B, 0x24, 0x00, 0x15, 0xBE, 0x9B, 0xFC, 0xDE, 0x7F, 0xED, 0x95, 0x1D, 0xD5, 0xAB, 0x2D, 0xCB
    };
    
    // try to load aeskeydb.bin file
    AesKeyInfo* keydb = (AesKeyInfo*) malloc(STD_BUFFER_SIZE);
    if (!keydb) return 1;
    u32 nkeys = LoadKeyDb(path, keydb, STD_BUFFER_SIZE);
    
    // compare with recommended SHA
    bool res = (nkeys && (sha_cmp(recommended_sha, keydb, nkeys * sizeof(AesKeyInfo), SHA256_MODE) == 0));
    
    free(keydb);
    return res ? 0 : 1;
}
