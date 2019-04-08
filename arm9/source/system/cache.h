#pragma once

#include "types.h"

static inline void
cpu_membarrier(void) {
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4\n\t"
        :: "r"(0) : "memory");
}

static inline void
cpu_invalidate_ic(void) {
    __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 0\n\t"
        :: "r"(0) : "memory");
}

static inline void
cpu_invalidate_ic_range(void *base, u32 len) {
    u32 addr = (u32)base & ~0x1F;
    len >>= 5;

    do {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1\n\t"
            :: "r"(addr) : "memory");

        addr += 0x20;
    } while(len--);
}

static inline void
cpu_invalidate_dc(void) {
    __asm__ __volatile__("mcr p15, 0, %0, c7, c6, 0\n\t"
        :: "r"(0) : "memory");
}

static inline void
cpu_invalidate_dc_range(void *base, u32 len) {
    u32 addr = (u32)base & ~0x1F;
    len >>= 5;

    do {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c6, 1"
            :: "r"(addr) : "memory");
        addr += 0x20;
    } while(len--);
}

static inline void
cpu_writeback_dc(void) {
    u32 seg = 0, ind;
    do {
        ind = 0;
        do {
            __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 2\n\t"
               :: "r"(seg | ind) : "memory");

            ind += 0x20;
        } while(ind < 0x400);
        seg += 0x40000000;
    } while(seg != 0);
}

static inline
void cpu_writeback_dc_range(void *base, u32 len) {
    u32 addr = (u32)base & ~0x1F;
    len >>= 5;

    do {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1"
            :: "r"(addr) : "memory");

        addr += 0x20;
    } while(len--);
}

static inline
void cpu_writeback_invalidate_dc(void) {
    u32 seg = 0, ind;
    do {
        ind = 0;
        do {
            __asm__ __volatile__("mcr p15, 0, %0, c7, c14, 2\n\t"
                :: "r"(seg | ind) : "memory");

            ind += 0x20;
        } while(ind < 0x400);
        seg += 0x40000000;
    } while(seg != 0);
}

static inline
void cpu_writeback_invalidate_dc_range(void *base, u32 len) {
    u32 addr = (u32)base & ~0x1F;
    len >>= 5;

    do {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c14, 1"
            :: "r"(addr) : "memory");

        addr += 0x20;
    } while(len--);
}
