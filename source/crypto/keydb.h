#pragma once

#include "common.h"

#define KEYS_UNKNOWN 0
#define KEYS_RETAIL  1
#define KEYS_DEVKIT  2

typedef struct {
    u8   slot; // keyslot, 0x00...0x3F 
    char type; // type 'X' / 'Y' / 'N' for normalKey
    char id[10]; // key ID for special keys, all zero for standard keys
    u8   reserved[2]; // reserved space
    u8   isDevkitKey; // 0 for retail units / 1 for DevKit units
    u8   isEncrypted; // 0 if not / anything else if it is
    u8   key[16];
} __attribute__((packed)) AesKeyInfo;

u32 GetUnitKeysType(void);
u32 LoadKeyFromFile(u8* key, u32 keyslot, char type, char* id);
