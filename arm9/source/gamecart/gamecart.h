#pragma once

#include "common.h"

#define CART_NONE   0
#define CART_CTR    (1<<0)
#define CART_NTR    (1<<1)
#define CART_TWL    (1<<2)

#define MODC_AREA_SIZE  0x4000
#define PRIV_HDR_SIZE   0x50

typedef struct {
    u8  header[0x8000]; // NTR header + secure area / CTR header + private header
    u8  twl_header[0x8000]; // TWL header + modcrypt area / unused
    u32 cart_type;
    u32 cart_id;
    u64 cart_size;
    u64 data_size;
    u32 arm9i_rom_offset; // TWL specific
} __attribute__((packed)) CartData;

u32 GetCartName(char* name, CartData* cdata);
u32 InitCardRead(CartData* cdata);
u32 ReadCartSectors(void* buffer, u32 sector, u32 count, CartData* cdata);
u32 ReadCartBytes(void* buffer, u64 offset, u64 count, CartData* cdata);
u32 ReadCartPrivateHeader(void* buffer, u64 offset, u64 count, CartData* cdata);
