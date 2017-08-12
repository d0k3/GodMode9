/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#pragma once
#include <types.h>

#define asm __asm volatile

static inline u32 CPU_ReadCPSR(void)
{
    u32 cpsr;
    asm("mrs %0, cpsr\n\t":"=r"(cpsr));
    return cpsr;
}

static inline void CPU_WriteCPSR_c(u32 cpsr)
{
    asm("msr cpsr_c, %0\n\t"::"r"(cpsr));
    return;
}

static inline u32 CPU_ReadCR(void)
{
    u32 cr;
    asm("mrc p15, 0, %0, c1, c0, 0\n\t":"=r"(cr));
    return cr;
}

static inline void CPU_WriteCR(u32 cr)
{
    asm("mcr p15, 0, %0, c1, c0, 0\n\t"::"r"(cr));
    return;
}

static inline void CPU_DisableIRQ(void)
{
    #ifdef ARM9
    CPU_WriteCPSR_c(CPU_ReadCPSR() | (SR_IRQ | SR_FIQ));
    #else
    asm("cpsid if\n\t");
    #endif
    return;
}

static inline void CPU_EnableIRQ(void)
{
    #ifdef ARM9
    CPU_WriteCPSR_c(CPU_ReadCPSR() & ~(SR_IRQ | SR_FIQ));
    #else
    asm("cpsie if\n\t");
    #endif
    return;
}

static inline void CPU_EnterCritical(u32 *ss)
{
    *ss = CPU_ReadCPSR();
    CPU_DisableIRQ();
    return;
}

static inline void CPU_LeaveCritical(u32 *ss)
{
    CPU_WriteCPSR_c(*ss);
    return;
}
