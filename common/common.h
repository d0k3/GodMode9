#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>

#include <types.h>

#ifdef ARM9
# ifdef MONITOR_HEAP
#include "mymalloc.h"
#define malloc my_malloc
#define realloc my_realloc
#define free my_free
# endif
#endif

#define max(a,b) \
    (((a) > (b)) ? (a) : (b))

#define min(a,b) \
    (((a) < (b)) ? (a) : (b))

#define abs(x) \
    (((x) >= 0) ? (x) : -(x))

#define clamp(x, min, max) \
    ((x) < (max) ? ((x) > (min) ? (x) : (min)) : (max))

#define getbe16(d) \
    (((d)[0]<<8) | (d)[1])
#define getbe32(d) \
    ((((u32) getbe16(d))<<16) | ((u32) getbe16((d)+2)))
#define getbe64(d) \
    ((((u64) getbe32(d))<<32) | ((u64) getbe32((d)+4)))

#define getle16(d) \
    (((d)[1]<<8) | (d)[0])
#define getle32(d) \
    ((((u32) getle16((d)+2))<<16) | ((u32) getle16(d)))
#define getle64(d) \
    ((((u64) getle32((d)+4))<<32) | ((u64) getle32(d)))

#define align(v,a) \
    (((v) % (a)) ? ((v) + (a) - ((v) % (a))) : (v))

#define countof(x) \
    (sizeof(x) / sizeof(*(x)))

#define bkpt \
    do{__builtin_trap(); __builtin_unreachable();}while(0)

#define assert(x) \
    (!!(x) ? (void)0 : __builtin_trap())

#define STATIC_ASSERT(...) \
    _Static_assert((__VA_ARGS__), #__VA_ARGS__)

// standard output path (support file paths are in support.h)
#define OUTPUT_PATH     "0:/gm9/out"

// used in several places
#define STD_BUFFER_SIZE     0x100000 // must be a multiple of 0x200

// buffer area defines (in use by image.c, for RAMdrive)
#define RAMDRV_BUFFER       ((u8*)0x22800000) // top of STACK
#define RAMDRV_SIZE_O3DS    (0x5800000) // 88MB
#define RAMDRV_SIZE_N3DS    (0xD800000) // 216MB
