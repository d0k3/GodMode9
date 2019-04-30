#pragma once

#include "common.h"


typedef struct {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u32 id; // shouldn't be zero
} __attribute__((packed)) TouchBox;

// this assumes the touchscreen is actually in use
bool TouchBoxGet(u16* x, u16* y, u32* id, const TouchBox* tbs, const u32 tbn);
