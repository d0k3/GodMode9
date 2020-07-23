#pragma once

#include "types.h"

/* Status Register flags */
#define SR_USR_MODE	(0x10)
#define SR_FIQ_MODE	(0x11)
#define SR_IRQ_MODE	(0x12)
#define SR_SVC_MODE	(0x13)
#define SR_ABT_MODE	(0x17)
#define SR_UND_MODE	(0x1B)
#define SR_SYS_MODE	(0x1F)
#define SR_PMODE_MASK	(0x0F)

#define SR_THUMB	BIT(5)
#define SR_NOFIQ	BIT(6)
#define SR_NOIRQ	BIT(7)
#define SR_NOINT	(SR_NOFIQ | SR_NOIRQ)

#ifdef ARM9
	#define CPU_FREQ	(134055928)
	#define CR_MPU		BIT(0)
	#define CR_DCACHE	BIT(2)
	#define CR_ICACHE	BIT(12)
	#define CR_DTCM		BIT(16)
	#define CR_ITCM		BIT(18)
	#define CR_V4TLD	BIT(15)

	#define CR_ALT_VECTORS	BIT(13)
	#define CR_CACHE_RROBIN	BIT(14)
	#define CR_DTCM_LOAD	BIT(17)
	#define CR_ITCM_LOAD	BIT(19)

	#define CR_TCM_LOAD	(CR_DTCM_LOAD | CR_ITCM_LOAD)

	#define ICACHE_SZ	(4096)
	#define DCACHE_SZ	(4096)

	#define MAX_IRQ	(32)
	#define MAX_CPU	(1)
#else // ARM11
	#define CPU_FREQ	(268111856)
	#define CR_MMU		BIT(0)
	#define CR_ALIGN	BIT(1)
	#define CR_DCACHE	BIT(2)
	#define CR_ICACHE	BIT(12)
	#define CR_FLOWPRED	BIT(11)
	#define CR_HIGHVEC	BIT(13)
	#define CR_V4TLD	BIT(15)
	#define CR_UNALIGN	BIT(22)
	#define CR_DSUBPAGE	BIT(23)

	#define ACR_RETSTK	BIT(0)
	#define ACR_DBPRED	BIT(1)
	#define ACR_SBPRED	BIT(2)
	#define ACR_FOLDING	BIT(3)
	#define ACR_EXCL	BIT(4)
	#define ACR_SMP		BIT(5)

	#define ICACHE_SZ	(16384)
	#define DCACHE_SZ	(16384)

	#define MAX_IRQ	(224)
	#define MAX_CPU	(1)
#endif

#define CR_CACHES	(CR_DCACHE | CR_ICACHE)


#ifndef __ASSEMBLER__

// only accessible from ARM mode
#define ARM_MCR(cp, op1, reg, crn, crm, op2)	asm_v( \
	"MCR " #cp ", " #op1 ", %[R], " #crn ", " #crm ", " #op2 "\n\t" \
	:: [R] "r"(reg) : "memory","cc")

#define ARM_MRC(cp, op1, reg, crn, crm, op2)	asm_v( \
	"MRC " #cp ", " #op1 ", %[R], " #crn ", " #crm ", " #op2 "\n\t" \
	: [R] "=r"(reg) :: "memory","cc")

#define ARM_MSR(cp, reg)	asm_v( \
	"MSR " #cp ", %[R]\n\t" \
	:: [R] "r"(reg) : "memory","cc")

#define ARM_MRS(reg, cp)	asm_v( \
	"MRS %[R], " #cp "\n\t" \
	: [R] "=r"(reg) :: "memory","cc")


/* ARM Private Memory Region */
#ifdef ARM11
	#define REG_ARM_PMR(off, type)	((volatile type*)(0x17E00000 + (off)))

	void ARM_ISB(void);
	void ARM_DMB(void);
	void ARM_WFI(void);
	void ARM_WFE(void);
	void ARM_SEV(void);
	u32 ARM_GetACR(void);
	void ARM_SetACR(u32 acr);
#endif

// Data Synchronization Barrier
void ARM_DSB(void);

// Get and set Control Register
u32 ARM_GetCR(void);
void ARM_SetCR(u32 cr);

// Get and set Thread ID
u32 ARM_GetTID(void);
void ARM_SetTID(u32 tid);

// Core ID (not CPU ID)
u32 ARM_CoreID(void);

// Get and set CPSR
u32 ARM_GetCPSR(void);
void ARM_SetCPSR_c(u32 sr);

// Manage interrupts
void ARM_DisableInterrupts(void);
void ARM_EnableInterrupts(void);
u32 ARM_EnterCritical(void);
void ARM_LeaveCritical(u32 stat);

void ARM_InvIC(void);
void ARM_InvIC_Range(void *base, u32 len);
void ARM_InvDC(void);
void ARM_InvDC_Range(void *base, u32 len);
void ARM_WbDC(void);
void ARM_WbDC_Range(void *base, u32 len);
void ARM_WbInvDC(void);
void ARM_WbInvDC_Range(void *base, u32 len);

static inline void ARM_BKPT(void) {
	__builtin_trap();
}

#endif // __ASSEMBLER__
