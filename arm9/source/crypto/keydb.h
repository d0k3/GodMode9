#pragma once

#include "common.h"

#define KEYDB_NAME "aeskeydb.bin"

// SHA-256 and size of the recommended aeskeydb.bin file
// equals MD5 A5B28945A7C051D7A0CD18AF0E580D1B / 1024 byte
#define KEYDB_PERFECT_HASH \
    0x40, 0x76, 0x54, 0x3D, 0xA3, 0xFF, 0x91, 0x1C, 0xE1, 0xCC, 0x4E, 0xC7, 0x2F, 0x92, 0xE4, 0xB7, \
    0x2B, 0x24, 0x00, 0x15, 0xBE, 0x9B, 0xFC, 0xDE, 0x7F, 0xED, 0x95, 0x1D, 0xD5, 0xAB, 0x2D, 0xCB
#define KEYDB_PERFECT_SIZE  (32 * sizeof(AesKeyInfo)) // 32 keys contained

#define KEYS_UNKNOWN 0
#define KEYS_DEVKIT  1
#define KEYS_RETAIL  2
    

typedef struct {
    u8   slot; // keyslot, 0x00...0x3F 
    char type; // type 'X' / 'Y' / 'N' for normalKey / 'I' for IV
    char id[10]; // key ID for special keys, all zero for standard keys
    u8   reserved[2]; // reserved space
    u8   keyUnitType; // 0 for ALL units / 1 for devkit exclusive / 2 for retail exclusive
    u8   isEncrypted; // 0 if not / anything else if it is
    u8   key[16];
} __attribute__((packed)) AesKeyInfo;

u32 GetUnitKeysType(void);
void CryptAesKeyInfo(AesKeyInfo* info);
u32 LoadKeyFromFile(void* key, u32 keyslot, char type, char* id);
u32 InitKeyDb(const char* path);
u32 CheckRecommendedKeyDb(const char* path);
