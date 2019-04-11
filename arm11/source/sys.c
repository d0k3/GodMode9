#include <types.h>
#include <arm.h>

#include "arm/gic.h"
#include "arm/scu.h"
#include "arm/mmu.h"

#define CFG11_MPCORE_CLKCNT	((vu16*)(0x10141300))
#define CFG11_SOCINFO	((vu16*)(0x10140FFC))

static bool SYS_IsNewConsole(void)
{
	return (*CFG11_SOCINFO & 2) != 0;
}

static bool SYS_ClkMultEnabled(void)
{
	return (*CFG11_MPCORE_CLKCNT & 1) != 0;
}

static void SYS_EnableClkMult(void)
{
	// magic bit twiddling to enable extra FCRAM
	// only done on N3DS and when it hasn't been done yet
	// state might get a bit messed up so it has to be done
	// as early as possible in the initialization chain
	if (SYS_IsNewConsole() && !SYS_ClkMultEnabled()) {
		GIC_Enable(88, BIT(0), GIC_HIGHEST_PRIO, NULL);
		ARM_EnableInterrupts();
		*CFG11_MPCORE_CLKCNT = 0x8001;
		do {
			ARM_WFI();
		} while(!(*CFG11_MPCORE_CLKCNT & 0x8000));
		ARM_DisableInterrupts();
		GIC_Disable(88, BIT(0));
	}
}

static void SYS_CoreZeroInit(void)
{
	GIC_GlobalReset();
	SYS_EnableClkMult();

	SCU_Init();

	// Init MMU tables here
	MMU_PopulateTranslationTable();
}

void SYS_CoreInit(void)
{
	if (!ARM_CoreID()) {
		SYS_CoreZeroInit();
	}

	GIC_LocalReset();

	MMU_Init();

	// enable fancy ARM11 stuff
	ARM_SetACR(ARM_GetACR() |
		ACR_RETSTK | ACR_DBPRED | ACR_SBPRED | ACR_FOLDING | ACR_SMP);

	ARM_SetCR(ARM_GetCR() |
		CR_MMU | CR_CACHES | CR_FLOWPRED | CR_HIGHVEC | CR_DSUBPAGE);

	ARM_DSB();
}
