#pragma once

#include "common.h"

#define REG_SHACNT      ((volatile uint32_t*)0x1000A000)
#define REG_SHABLKCNT   ((volatile uint32_t*)0x1000A004)
#define REG_SHAHASH     ((volatile uint32_t*)0x1000A040)
#define REG_SHAINFIFO   ((volatile uint32_t*)0x1000A080)

#define SHA_CNT_STATE           0x00000003
#define SHA_CNT_OUTPUT_ENDIAN   0x00000008
#define SHA_CNT_MODE            0x00000030
#define SHA_CNT_ENABLE          0x00010000
#define SHA_CNT_ACTIVE          0x00020000

#define SHA_HASH_READY          0x00000000
#define SHA_NORMAL_ROUND        0x00000001
#define SHA_FINAL_ROUND         0x00000002

#define SHA256_MODE             0
#define SHA224_MODE             0x00000010
#define SHA1_MODE               0x00000020


void sha_init(u32 mode);
void sha_update(const void* src, u32 size);
void sha_get(void* res);
void sha_quick(void* res, const void* src, u32 size, u32 mode);
