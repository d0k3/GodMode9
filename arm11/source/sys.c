#include <types.h>
#include <arm.h>
#include <pxi.h>

#include "arm/gic.h"
#include "arm/scu.h"
#include "arm/mmu.h"

#include "hw/i2c.h"

#define CFG11_MPCORE_CLKCNT	((vu16*)(0x10141300))
#define CFG11_SOCINFO		((vu16*)(0x10140FFC))

#define LEGACY_BOOT_ENTRY	((vu32*)0x1FFFFFFC)

#define INIT_DONE	(0xDDEEFFAA)

static volatile u32 sys_init_state = 0;

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

#include "sections.h"

#define	MMU_FLAGS_DEF	MMU_FLAGS(STRONGLY_ORDERED, READ_WRITE, 0, 1)

static void SYS_CoreZeroInit(void)
{
	GIC_GlobalReset();
	GIC_LocalReset();

	*LEGACY_BOOT_ENTRY = 0;

	SYS_EnableClkMult();

	SCU_Init();

	// Map all sections here
	MMU_Map(SECTION_TRI(vector), MMU_FLAGS(CACHED_WT, READ_ONLY, 0, 0));
	MMU_Map(SECTION_TRI(text), MMU_FLAGS(CACHED_WT, READ_ONLY, 0, 1));
	MMU_Map(SECTION_TRI(data), MMU_FLAGS(CACHED_WB_ALLOC, READ_WRITE, 1, 1));
	MMU_Map(SECTION_TRI(rodata), MMU_FLAGS(CACHED_WT, READ_ONLY, 1, 1));
	MMU_Map(SECTION_TRI(bss), MMU_FLAGS(CACHED_WB_ALLOC, READ_WRITE, 1, 1));

	// IO Registers
	MMU_Map(0x10100000, 0x10100000, 4UL << 20, MMU_FLAGS(DEVICE_SHARED, READ_WRITE, 1, 1));

	// MPCore Private Memory Region
	MMU_Map(0x17E00000, 0x17E00000, 8UL << 10, MMU_FLAGS(DEVICE_SHARED, READ_WRITE, 1, 1));

	// VRAM
	MMU_Map(0x18000000, 0x18000000, 6UL << 20, MMU_FLAGS(CACHED_WT, READ_WRITE, 1, 1));

	// FCRAM
	MMU_Map(0x20000000, 0x20000000, 128UL << 20, MMU_FLAGS(CACHED_WB, READ_WRITE, 1, 1));
}

static void SYS_InitPeripherals(void)
{
	PXI_Reset();
	I2C_init();
	//MCU_init();
}

void SYS_CoreInit(void)
{
	if (!ARM_CoreID()) {
		SYS_CoreZeroInit();
	} else {
		while(sys_init_state != INIT_DONE)
			ARM_WFE();

		GIC_LocalReset();
	}

	// set up MMU registers
	MMU_Init();

	// enable fancy ARM11 stuff
	ARM_SetACR(ARM_GetACR() |
		ACR_RETSTK | ACR_DBPRED | ACR_SBPRED | ACR_FOLDING | ACR_SMP);

	ARM_SetCR(ARM_GetCR() |
		CR_MMU | CR_CACHES | CR_FLOWPRED | CR_HIGHVEC | CR_DSUBPAGE);

	ARM_DSB();

	if (!ARM_CoreID()) {
		SYS_InitPeripherals();

		sys_init_state = INIT_DONE;
		ARM_DSB();
		ARM_SEV();
	}
}

// assumes all cores have been initialized
void SYS_CoreShutdown(void)
{
	u32 core = ARM_CoreID();

	if (!core) {
		// wait for the other cores to do their thing
		while(sys_init_state != (INIT_DONE - MAX_CPU + 1)) {
			ARM_WFE();
			ARM_DSB();
		}

		GIC_GlobalReset();
	} else {
		__atomic_sub_fetch(&sys_init_state, 1, __ATOMIC_SEQ_CST);
		ARM_SEV();
	}

	GIC_LocalReset();

	ARM_WbInvDC();
	ARM_InvIC();
	ARM_DSB();

	ARM_SetCR(ARM_GetCR() & ~(CR_MMU | CR_CACHES | CR_FLOWPRED));
	ARM_SetACR(ARM_GetACR() &
		~(ACR_RETSTK | ACR_DBPRED | ACR_SBPRED | ACR_FOLDING | ACR_SMP));

	if (!core) {
		while(*LEGACY_BOOT_ENTRY == 0);
		((void (*)(void))(*LEGACY_BOOT_ENTRY))();
	} else {
		// Branch to bootrom function that does SMP reinit magic
		// (waits for IPI + branches to word @ 0x1FFFFFDC)
		((void (*)(void))0x0001004C)();
	}
}
