#pragma once

#include "common.h"

#define CIFINISH_MAGIC			"CIFINISH"
#define CIFINISH_TITLE_MAGIC	"TITLE"
#define CIFINISH_SIZE(c)		(sizeof(CifinishHeader) + ((((CifinishHeader*)(c))->n_entries) * sizeof(CifinishTitle)))

// see: https://github.com/ihaveamac/custom-install/blob/ac0be9d61d7ebef9356df23036dc53e8e862011a/custominstall.py#L163
typedef struct {
    char magic[8];
    u32 version;
    u32 n_entries;
} __attribute__((packed, aligned(4))) CifinishHeader;

typedef struct {
    char magic[5];
    u8  padding0;
    u8  has_seed; // 1 if it does, otherwise 0
    u8  padding1;
    u64 title_id;
    u8  seed[16];
} __attribute__((packed, aligned(4))) CifinishTitle;
