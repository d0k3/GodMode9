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
#define SR_PMODE_MASK	(0x1F)

#define SR_THUMB	BIT(5)
#define SR_NOFIQ	BIT(6)
#define SR_NOIRQ	BIT(7)
#define SR_NOINT	(SR_NOFIQ | SR_NOIRQ)

#ifdef ARM9
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

/* ARM Private Memory Region */
#ifdef ARM11
	#define REG_ARM_PMR(off, type)	((volatile type*)(0x17E00000 + (off)))
#endif


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

#ifdef ARM11
	#define ARM_CPS(m)	asm_v("CPS " #m)
	#define ARM_CPSID(m)	asm_v("CPSID " #m)
	#define ARM_CPSIE(m)	asm_v("CPSIE " #m)

	/*
	 * An Instruction Synchronization Barrier (ISB) flushes the pipeline in the processor
	 * so that all instructions following the ISB are fetched from cache or memory
	 * after the ISB has been completed.
	 */
	static inline void ARM_ISB(void) {
		ARM_MCR(p15, 0, 0, c7, c5, 4);
	}

	/*
	 * A Data Memory Barrier (DMB) ensures that all explicit memory accesses before
	 * the DMB instruction complete before any explicit memory accesses after the DMB instruction start.
	 */
	static inline void ARM_DMB(void) {
		ARM_MCR(p15, 0, 0, c7, c10, 5);
	}

	/* Wait For Interrupt */
	static inline void ARM_WFI(void) {
		asm_v("wfi\n\t");
	}

	/* Wait For Event */
	static inline void ARM_WFE(void) {
		asm_v("wfe\n\t");
	}

	/* Send Event */
	static inline void ARM_SEV(void) {
		asm_v("sev\n\t");
	}

	/* Auxiliary Control Registers */
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
	ARM_MCR(p15, 0, 0, c7, c10, 4);
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

/* Thread ID Registers */
static inline u32 ARM_GetTID(void) {
	u32 pid;
	#ifdef ARM9
	ARM_MRC(p15, 0, pid, c13, c0, 1);
	#else
	ARM_MRC(p15, 0, pid, c13, c0, 4);
	#endif
	return pid;
}

static inline void ARM_SetTID(u32 pid) {
	#ifdef ARM9
	ARM_MCR(p15, 0, pid, c13, c0, 1);
	#else
	ARM_MCR(p15, 0, pid, c13, c0, 4);
	#endif
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

/* Status Register */
static inline u32 ARM_GetCPSR(void) {
	u32 sr;
	ARM_MRS(sr, cpsr);
	return sr;
}

static inline void ARM_SetCPSR_c(u32 sr) {
	ARM_MSR(cpsr_c, sr);
}

static inline void ARM_DisableInterrupts(void) {
	#ifdef ARM9
	ARM_SetCPSR_c(ARM_GetCPSR() | SR_NOINT);
	#else
	ARM_CPSID(if);
	#endif
}

static inline void ARM_EnableInterrupts(void) {
	#ifdef ARM9
	ARM_SetCPSR_c(ARM_GetCPSR() & ~SR_NOINT);
	#else
	ARM_CPSIE(if);
	#endif
}

static inline u32 ARM_EnterCritical(void) {
	u32 stat = ARM_GetCPSR();
	ARM_DisableInterrupts();
	return stat & SR_NOINT;
}

static inline void ARM_LeaveCritical(u32 stat) {
	ARM_SetCPSR_c((ARM_GetCPSR() & ~SR_NOINT) | stat);
}


/* Cache functions */
static inline void ARM_InvIC(void) {
	#ifdef ARM9
	ARM_MCR(p15, 0, 0, c7, c5, 0);
	#else
	ARM_MCR(p15, 0, 0, c7, c7, 0);
	#endif
}

static inline void ARM_InvIC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		#ifdef ARM9
		ARM_MCR(p15, 0, addr, c7, c5, 1);
		#else
		ARM_MCR(p15, 0, addr, c7, c7, 1);
		#endif
		addr += 0x20;
	} while(len--);
}

static inline void ARM_InvDC(void) {
	ARM_MCR(p15, 0, 0, c7, c6, 0);
}

static inline void ARM_InvDC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		ARM_MCR(p15, 0, addr, c7, c6, 1);
		addr += 0x20;
	} while(len--);
}

static inline void ARM_WbDC(void) {
	#ifdef ARM9
	u32 seg = 0, ind;
	do {
		ind = 0;
		do {
			ARM_MCR(p15, 0, seg | ind, c7, c10, 2);
			ind += 0x20;
		} while(ind < 0x400);
		seg += 0x40000000;
	} while(seg != 0);
	#else
	ARM_MCR(p15, 0, 0, c7, c10, 0);
	#endif
}

static inline void ARM_WbDC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		ARM_MCR(p15, 0, addr, c7, c10, 1);
		addr += 0x20;
	} while(len--);
}

static inline void ARM_WbInvDC(void) {
	#ifdef ARM9
	u32 seg = 0, ind;
	do {
		ind = 0;
		do {
			ARM_MCR(p15, 0, seg | ind, c7, c14, 2);
			ind += 0x20;
		} while(ind < 0x400);
		seg += 0x40000000;
	} while(seg != 0);
	#else
	ARM_MCR(p15, 0, 0, c7, c14, 0);
	#endif
}

static inline void ARM_WbInvDC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		ARM_MCR(p15, 0, addr, c7, c14, 1);
		addr += 0x20;
	} while(len--);
}

static inline void ARM_BKPT(void) {
	__builtin_trap();
}

#endif // __ASSEMBLER__
