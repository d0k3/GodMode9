#pragma once

#include "common.h"

#define BUFFER_ADDRESS  ((u8*) 0x21000000)
#define BUFFER_MAX_SIZE (1 * 1024 * 1024) // must be a multiple of 0x40 (64)

typedef struct {
    u32  keyslot;
    u32  setKeyY;
    u8   ctr[16];
    u8   keyY[16];
    u32  size;
    u32  mode;
    u8*  buffer;
} __attribute__((packed)) CryptBufferInfo;

typedef struct {
    u32  keyslot;
    u32  setKeyY;
    u8   ctr[16];
    u8   keyY[16];
    u32  size_mb;
    u32  mode;
    char filename[180];
} __attribute__((packed, aligned(16))) PadInfo;


u32 CryptBuffer(CryptBufferInfo *info);
u32 CreatePad(PadInfo *info);
