#pragma once

#ifndef ARM9
#ifndef ARM11
#error "Unknown processor"
#endif
#endif

#include <stdint.h>
#include <stddef.h>

#define BIT(x) (1<<(x))

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int64_t s8;
typedef int64_t s16;
typedef int64_t s32;
typedef int64_t s64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

typedef volatile s8  vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;
