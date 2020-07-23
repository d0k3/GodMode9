#include <arm.h>

#define ARM_TARGET __attribute__((noinline, target("arm")))

#ifdef ARM11
	#define ARM_CPS(m)	asm_v("CPS " #m)
	#define ARM_CPSID(m)	asm_v("CPSID " #m)
	#define ARM_CPSIE(m)	asm_v("CPSIE " #m)

	/*
	 * An Instruction Synchronization Barrier (ISB) flushes the pipeline in the processor
	 * so that all instructions following the ISB are fetched from cache or memory
	 * after the ISB has been completed.
	 */
	void ARM_TARGET ARM_ISB(void) {
		ARM_MCR(p15, 0, 0, c7, c5, 4);
	}

	/*
	 * A Data Memory Barrier (DMB) ensures that all explicit memory accesses before
	 * the DMB instruction complete before any explicit memory accesses after the DMB instruction start.
	 */
	void ARM_TARGET ARM_DMB(void) {
		ARM_MCR(p15, 0, 0, c7, c10, 5);
	}

	/* Wait For Interrupt */
	void ARM_TARGET ARM_WFI(void) {
		asm_v("wfi\n\t");
	}

	/* Wait For Event */
	void ARM_TARGET ARM_WFE(void) {
		asm_v("wfe\n\t");
	}

	/* Send Event */
	void ARM_TARGET ARM_SEV(void) {
		asm_v("sev\n\t");
	}

	/* Auxiliary Control Registers */
	u32 ARM_TARGET ARM_GetACR(void) {
		u32 acr;
		ARM_MRC(p15, 0, acr, c1, c0, 1);
		return acr;
	}

	void ARM_TARGET ARM_SetACR(u32 acr) {
		ARM_MCR(p15, 0, acr, c1, c0, 1);
	}
#endif


/*
 * A Data Synchronization Barrier (DSB) completes when all
 * instructions before this instruction complete.
 */
void ARM_TARGET ARM_DSB(void) {
	ARM_MCR(p15, 0, 0, c7, c10, 4);
}


/* Control Registers */
u32 ARM_TARGET ARM_GetCR(void) {
	u32 cr;
	ARM_MRC(p15, 0, cr, c1, c0, 0);
	return cr;
}

void ARM_TARGET ARM_SetCR(u32 cr) {
	ARM_MCR(p15, 0, cr, c1, c0, 0);
}

/* Thread ID Registers */
u32 ARM_TARGET ARM_GetTID(void) {
	u32 tid;
	#ifdef ARM9
	ARM_MRC(p15, 0, tid, c13, c0, 1);
	#else
	ARM_MRC(p15, 0, tid, c13, c0, 4);
	#endif
	return tid;
}

void ARM_TARGET ARM_SetTID(u32 tid) {
	#ifdef ARM9
	ARM_MCR(p15, 0, tid, c13, c0, 1);
	#else
	ARM_MCR(p15, 0, tid, c13, c0, 4);
	#endif
}

/* CPU ID */
u32 ARM_TARGET ARM_CoreID(void) {
	u32 id;
	#ifdef ARM9
	id = 0;
	#else
	ARM_MRC(p15, 0, id, c0, c0, 5);
	#endif
	return id & 3;
}

/* Status Register */
u32 ARM_TARGET ARM_GetCPSR(void) {
	u32 sr;
	ARM_MRS(sr, cpsr);
	return sr;
}

void ARM_TARGET ARM_SetCPSR_c(u32 sr) {
	ARM_MSR(cpsr_c, sr);
}

void ARM_TARGET ARM_DisableInterrupts(void) {
	#ifdef ARM9
	ARM_SetCPSR_c(ARM_GetCPSR() | SR_NOINT);
	#else
	ARM_CPSID(if);
	#endif
}

void ARM_TARGET ARM_EnableInterrupts(void) {
	#ifdef ARM9
	ARM_SetCPSR_c(ARM_GetCPSR() & ~SR_NOINT);
	#else
	ARM_CPSIE(if);
	#endif
}

u32 ARM_TARGET ARM_EnterCritical(void) {
	u32 stat = ARM_GetCPSR();
	ARM_DisableInterrupts();
	return stat & SR_NOINT;
}

void ARM_TARGET ARM_LeaveCritical(u32 stat) {
	ARM_SetCPSR_c((ARM_GetCPSR() & ~SR_NOINT) | stat);
}


/* Cache functions */
void ARM_TARGET ARM_InvIC(void) {
	#ifdef ARM9
	ARM_MCR(p15, 0, 0, c7, c5, 0);
	#else
	ARM_MCR(p15, 0, 0, c7, c7, 0);
	#endif
}

void ARM_TARGET ARM_InvIC_Range(void *base, u32 len) {
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

void ARM_TARGET ARM_InvDC(void) {
	ARM_MCR(p15, 0, 0, c7, c6, 0);
}

void ARM_TARGET ARM_InvDC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		ARM_MCR(p15, 0, addr, c7, c6, 1);
		addr += 0x20;
	} while(len--);
}

void ARM_TARGET ARM_WbDC(void) {
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

void ARM_TARGET ARM_WbDC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		ARM_MCR(p15, 0, addr, c7, c10, 1);
		addr += 0x20;
	} while(len--);
}

void ARM_TARGET ARM_WbInvDC(void) {
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

void ARM_TARGET ARM_WbInvDC_Range(void *base, u32 len) {
	u32 addr = (u32)base & ~0x1F;
	len >>= 5;

	do {
		ARM_MCR(p15, 0, addr, c7, c14, 1);
		addr += 0x20;
	} while(len--);
}
