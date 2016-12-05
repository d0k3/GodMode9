#include "keydb.h"
#include "aes.h"
#include "sha.h"
#include "ff.h"

#define KEYDB_NAME "aeskeydb.bin"

typedef struct {
    u8   slot;           // keyslot, 0x00...0x39 
    char type;           // type 'X' / 'Y' / 'N' for normalKey
    char id[10];         // key ID for special keys, all zero for standard keys
} __attribute__((packed)) AesKeyDesc;

typedef struct {
    AesKeyDesc desc;     // slot, type, id
    u8   isDevkitKey;     // 1 if for DevKit unit, 0 otherwise
    u8   keySha256[32];  // SHA-256 of the key
} __attribute__((packed)) AesKeyHashInfo;

typedef struct {
    u8   slot;           // keyslot, 0x00...0x39
    u8   isDevkitKey;     // 1 if for DevKit unit, 0 otherwise
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

u32 CheckAesKeyInfo(u8* key, u32 keyslot, char type, char* id)
{
    static const AesKeyHashInfo keyHashes[] = {
        { { 0x05, 'Y', "" }, 0, // Retail N3DS CTRNAND key SHA256
         { 0x98, 0x24, 0x27, 0x14, 0x22, 0xB0, 0x6B, 0xF2, 0x10, 0x96, 0x9C, 0x36, 0x42, 0x53, 0x7C, 0x86,
         0x62, 0x22, 0x5C, 0xFD, 0x6F, 0xAE, 0x9B, 0x0A, 0x85, 0xA5, 0xCE, 0x21, 0xAA, 0xB6, 0xC8, 0x4D }
        },
        { { 0x18, 'X', "" }, 0, // Retail NCCH Secure3 key SHA256
         { 0x76, 0xC7, 0x6B, 0x65, 0x5D, 0xB8, 0x52, 0x19, 0xC5, 0xD3, 0x5D, 0x51, 0x7F, 0xFA, 0xF7, 0xA4,
         0x3E, 0xBA, 0xD6, 0x6E, 0x31, 0xFB, 0xDD, 0x57, 0x43, 0x92, 0x59, 0x37, 0xA8, 0x93, 0xCC, 0xFC }
        },
        { { 0x1B, 'X', "" }, 0, // Retail NCCH Secure4 key SHA256
         { 0x9A, 0x20, 0x1E, 0x7C, 0x37, 0x37, 0xF3, 0x72, 0x2E, 0x5B, 0x57, 0x8D, 0x11, 0x83, 0x7F, 0x19,
         0x7C, 0xA6, 0x5B, 0xF5, 0x26, 0x25, 0xB2, 0x69, 0x06, 0x93, 0xE4, 0x16, 0x53, 0x52, 0xC6, 0xBB }
        },
        { { 0x25, 'X', "" }, 0, // Retail NCCH 7x key SHA256
         { 0x7E, 0x87, 0x8D, 0xDE, 0x92, 0x93, 0x8E, 0x4C, 0x71, 0x7D, 0xD5, 0x3D, 0x1E, 0xA3, 0x5A, 0x75,
         0x63, 0x3F, 0x51, 0x30, 0xD8, 0xCF, 0xD7, 0xC7, 0x6C, 0x8F, 0x4A, 0x8F, 0xB8, 0x70, 0x50, 0xCD }
        }/*, 
        { { 0x18, 'X', "" }, 1, // DevKit NCCH Secure3 key SHA256
         { 0x08, 0xE1, 0x09, 0x62, 0xF6, 0x5A, 0x09, 0xAA, 0x12, 0x2C, 0x7C, 0xBE, 0xDE, 0xA1, 0x9C, 0x4B,
         0x5C, 0x9A, 0x8A, 0xC3, 0xD9, 0x8E, 0xA1, 0x62, 0x04, 0x11, 0xD7, 0xE8, 0x55, 0x70, 0xA6, 0xC2 }
        },
        { { 0x1B, 'X', "" }, 1, // DevKit NCCH Secure4 key SHA256
         { 0xA5, 0x3C, 0x3E, 0x5D, 0x09, 0x5C, 0x73, 0x35, 0x21, 0x79, 0x3F, 0x2E, 0x4C, 0x10, 0xCA, 0xAE,
         0x87, 0x83, 0x51, 0x53, 0x46, 0x0B, 0x52, 0x39, 0x9B, 0x00, 0x62, 0xF6, 0x39, 0xCB, 0x62, 0x16 }
        }*/
    };
    
    u8 keySha256[32];
    sha_quick(keySha256, key, 16, SHA256_MODE);
    for (u32 p = 0; p < sizeof(keyHashes) / sizeof(AesKeyHashInfo); p++) {
        if ((keyHashes[p].desc.slot != keyslot) || (keyHashes[p].desc.type != type))
            continue;
        if ((!id && keyHashes[p].desc.id[0]) || (id && strncmp(id, keyHashes[p].desc.id, 10) != 0))
            continue;
        if ((bool) keyHashes[p].isDevkitKey != (GetUnitKeysType() == KEYS_DEVKIT))
            continue;
        if (memcmp(keySha256, keyHashes[p].keySha256, 32) == 0) {
            return 0;
        }
    }
    
    return 1;
}

u32 CheckKeySlot(u32 keyslot, char type)
{
    static const AesNcchSampleInfo keyNcchSamples[] = {
        { 0x18, 0, // Retail NCCH Secure3
         { 0x78, 0xBB, 0x84, 0xFA, 0xB3, 0xA2, 0x49, 0x83, 0x9E, 0x4F, 0x50, 0x7B, 0x17, 0xA0, 0xDA, 0x23 } },
        { 0x1B, 0, // Retail NCCH Secure4
         { 0xF3, 0x6F, 0x84, 0x7E, 0x59, 0x43, 0x6E, 0xD5, 0xA0, 0x40, 0x4C, 0x71, 0x19, 0xED, 0xF7, 0x0A } },
        { 0x25, 0, // Retail NCCH 7x
         { 0x34, 0x7D, 0x07, 0x48, 0xAE, 0x5D, 0xFB, 0xB0, 0xF5, 0x86, 0xD6, 0xB5, 0x14, 0x65, 0xF1, 0xFF } },
        { 0x18, 1, // DevKit NCCH Secure3
         { 0x20, 0x8B, 0xB5, 0x61, 0x94, 0x18, 0x6A, 0x84, 0x91, 0xD6, 0x37, 0x27, 0x91, 0xF3, 0x53, 0xC9 } },
        { 0x1B, 1, // DevKit NCCH Secure4
         { 0xB3, 0x9D, 0xC1, 0xDB, 0x5B, 0x0C, 0xE7, 0x60, 0xBE, 0xAD, 0x5A, 0xBF, 0xD0, 0x86, 0x99, 0x88 } },
        { 0x25, 1, // DevKit NCCH 7x
         { 0xBC, 0x83, 0x7C, 0xC9, 0x99, 0xC8, 0x80, 0x9E, 0x8A, 0xDE, 0x4A, 0xFA, 0xAA, 0x72, 0x08, 0x28 } }
    };
    u64* state = (type == 'X') ? &keyXState : (type == 'Y') ? &keyYState : &keyState;
    
    // just to be safe...
    if (keyslot >= 0x40)
        return 1;
    
    // check if key is already loaded
    if ((*state >> keyslot) & 1)
        return 0;
    
    // if is not, we may still be able to verify the currently set one (for NCCH keys)
    for (u32 p = 0; (type == 'X') && (p < sizeof(keyNcchSamples) / sizeof(AesNcchSampleInfo)); p++) {
        if (keyNcchSamples[p].slot != keyslot) // only for keyslots in the keyNcchSamples table!
            continue;
        if ((bool) keyNcchSamples[p].isDevkitKey != (GetUnitKeysType() == KEYS_DEVKIT))
            continue;
        u8 zeroes[16] = { 0 };
        u8 sample[16] = { 0 };
        setup_aeskeyY(keyslot, zeroes);
        use_aeskey(keyslot);
        set_ctr(zeroes);
        aes_decrypt(sample, sample, 1, AES_CNT_CTRNAND_MODE);
        if (memcmp(keyNcchSamples[p].sample, sample, 16) == 0) {
            keyXState |= (u64) 1 << keyslot;
            return 0;
        }
    }
    
    // not set up if getting here
    return 1;
}

u32 LoadKeyFromFile(u8* key, u32 keyslot, char type, char* id)
{
    const char* base[] = { INPUT_PATHS };
    u8 keystore[16] = {0};
    bool found = false;
    
    // use keystore if key == NULL
    if (!key) key = keystore;
    
    // checking the obvious
    if ((keyslot >= 0x40) || ((type != 'X') && (type != 'Y') && (type != 'N')))
        return 1;
    
    // check if already loaded
    if (!id && (CheckKeySlot(keyslot, type) == 0)) return 0;
    // try to get key from 'aeskeydb.bin' file
    for (u32 i = 0; !found && (i < (sizeof(base)/sizeof(char*))); i++) {
        FIL fp;
        char path[64];
        AesKeyInfo info;
        UINT btr;
        snprintf(path, 64, "%s/%s", base[i], KEYDB_NAME);
        if (f_open(&fp, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) continue;
        while ((f_read(&fp, &info, sizeof(AesKeyInfo), &btr) == FR_OK) && (btr == sizeof(AesKeyInfo))) {
            if ((info.slot == keyslot) && (info.type == type) && 
                ((!id && !(info.id[0])) || (id && (strncmp(id, info.id, 10) == 0)))) {
                found = true;
                if (info.isEncrypted)
                    CryptAesKeyInfo(&info);
                memcpy(key, info.key, 16);
                break;
            }
        }
        f_close(&fp);
    }
    
    // load legacy slot0x??Key?.bin file instead
    if (!found) {
        for (u32 i = 0; !found && (i < (sizeof(base)/sizeof(char*))); i++) {
            FIL fp;
            char path[64];
            UINT btr;
            snprintf(path, 64, "%s/slot0x%02lXKey%s.bin", base[i], keyslot,
                (id) ? id : (type == 'X') ? "X" : (type == 'Y') ? "Y" : "");
            if (f_open(&fp, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) continue;
            if ((f_read(&fp, key, 16, &btr) == FR_OK) && (btr == 16)) {
                found = true;
                break;
            }
            f_close(&fp);
        }
    }
    
    // key still not found (duh)
    if (!found) return 1; // out of options here
    
    // verify key (verification is enforced)
    if (CheckAesKeyInfo(key, keyslot, type, id) != 0) return 1;
    
    // now, setup the key
    if (type == 'X') {
        setup_aeskeyX(keyslot, key);
        keyXState |= (u64) 1 << keyslot;
    } else if (type == 'Y') {
        setup_aeskeyY(keyslot, key);
        keyYState |= (u64) 1 << keyslot;
    } else { // normalKey includes keyX & keyY
        setup_aeskey(keyslot, key);
        keyState  |= (u64) 1 << keyslot;
        keyXState |= (u64) 1 << keyslot;
        keyYState |= (u64) 1 << keyslot;
    }
    use_aeskey(keyslot);
    
    return 0;
}
