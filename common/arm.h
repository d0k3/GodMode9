#pragma once

#include "types.h"

#include <bfn.h>

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

	static inline void ARM_ISB(void) {
		((void (*)(void))(BFN_INSTSYNCBARRIER))();
	}

	static inline void ARM_DMB(void) {
		((void (*)(void))(BFN_DATAMEMBARRIER))();
	}

	static inline void ARM_WFI(void) {
		// replace with a bootrom call if
		// switching to thumb is necessary
		asm_v("wfi\n\t":::"memory");
	}

	static inline void ARM_WFE(void) {
		asm_v("wfe\n\t":::"memory"); // same as above
	}

	static inline void ARM_SEV(void) {
		asm_v("sev\n\t":::"memory"); // same as above
	}

	/* Control Registers */
	static inline u32 ARM_GetCR(void) {
		u32 cr;
		ARM_MRC(p15, 0, cr, c1, c0, 0);
		return cr;
	}

	static inline void ARM_SetCR(u32 cr) {
		ARM_MCR(p15, 0, cr, c1, c0, 0);
	}

	static inline u32 ARM_GetACR(void) {
		u32 acr;
		ARM_MRC(p15, 0, acr, c1, c0, 1);
		return acr;
	}

	static inline void ARM_SetACR(u32 acr) {
		ARM_MCR(p15, 0, acr, c1, c0, 1);
	}

#endif

/*
 * A Data Synchronization Barrier (DSB) completes when all
 * instructions before this instruction complete.
 */
static inline void ARM_DSB(void) {
	((void (*)(void))(BFN_DATASYNCBARRIER))();
}

/* CPU ID */
static inline u32 ARM_CoreID(void) {
	u32 id;
	#ifdef ARM9
	id = 0;
	#else
	ARM_MRC(p15, 0, id, c0, c0, 5);
	#endif
	return id & 3;
}

/* Status register management */
static inline u32 ARM_EnterCritical(void) {
	return ((u32 (*)(void))(BFN_ENTERCRITICALSECTION))();
}

static inline void ARM_LeaveCritical(u32 stat) {
	((void (*)(u32))(BFN_LEAVECRITICALSECTION))(stat);
}

static inline void ARM_DisableInterrupts(void) {
	ARM_LeaveCritical(SR_NOINT);
}

static inline void ARM_EnableInterrupts(void) {
	ARM_LeaveCritical(0x00);
}

/* Cache functions */
static inline void ARM_InvIC(void) {
	((void (*)(void))(BFN_INVALIDATE_ICACHE))();
}

static inline void ARM_InvIC_Range(void *base, u32 len) {
	((void (*)(u32, u32))(BFN_INVALIDATE_ICACHE_RANGE))((u32)base, len);
	#ifdef ARM11 // make sure to also invalidate the branch target cache
	((void (*)(u32, u32))(BFN_INVALIDATE_BT_CACHE_RANGE))((u32)base, len);
	#endif
}

static inline void ARM_InvDC(void) {
	((void (*)(void))(BFN_INVALIDATE_DCACHE))();
}

static inline void ARM_InvDC_Range(void *base, u32 len) {
	((void (*)(u32, u32))(BFN_INVALIDATE_DCACHE_RANGE))((u32)base, len);
}

static inline void ARM_WbDC(void) {
	((void (*)(void))(BFN_WRITEBACK_DCACHE))();
}

static inline void ARM_WbDC_Range(void *base, u32 len) {
	((void (*)(u32, u32))(BFN_WRITEBACK_DCACHE_RANGE))((u32)base, len);
}

static inline void ARM_WbInvDC(void) {
	((void (*)(void))(BFN_WRITEBACK_INVALIDATE_DCACHE))();
}

static inline void ARM_WbInvDC_Range(void *base, u32 len) {
	((void (*)(u32, u32))(BFN_WRITEBACK_INVALIDATE_DCACHE_RANGE))((u32)base, len);
}

static inline void ARM_WaitCycles(u32 cycles) {
	((void (*)(u32))(BFN_WAITCYCLES))(cycles);
}

static inline void ARM_BKPT(void) {
	__builtin_trap();
}

#endif // __ASSEMBLER__
