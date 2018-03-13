#include "sha.h"

typedef struct
{
	u32 data[16];
} _sha_block;

void sha_init(u32 mode)
{
    while(*REG_SHACNT & 1);
    *REG_SHACNT = mode | SHA_CNT_OUTPUT_ENDIAN | SHA_NORMAL_ROUND;
}

void sha_update(const void* src, u32 size)
{    
    const u32* src32 = (const u32*)src;
    
    while(size >= 0x40) {
        while(*REG_SHACNT & 1);
        *((_sha_block*)REG_SHAINFIFO) = *((const _sha_block*)src32);
        src32 += 16;
        size -= 0x40;
    }
    while(*REG_SHACNT & 1);
    if(size) memcpy((void*)REG_SHAINFIFO, src32, size);
}

void sha_get(void* res) {
    u32 hash_size = (*REG_SHACNT&SHA224_MODE) ? (224/8) :
                    (*REG_SHACNT&SHA1_MODE) ? (160/8) : (256/8);
    *REG_SHACNT = (*REG_SHACNT & ~SHA_NORMAL_ROUND) | SHA_FINAL_ROUND;
    while(*REG_SHACNT & SHA_FINAL_ROUND);
    while(*REG_SHACNT & 1);
    if (hash_size) memcpy(res, (void*)REG_SHAHASH, hash_size);
}

void sha_quick(void* res, const void* src, u32 size, u32 mode) {
    sha_init(mode);
    sha_update(src, size);
    sha_get(res);
}

int sha_cmp(const void* sha, const void* src, u32 size, u32 mode) {
    u8 res[0x20];
    sha_quick(res, src, size, mode);
    return memcmp(sha, res, 0x20);
}
