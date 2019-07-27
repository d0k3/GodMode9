/*
 *   This file is part of GodMode9
 *   Copyright (C) 2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <types.h>
#include <vram.h>
#include <arm.h>
#include <pxi.h>

#include "arm/gic.h"
#include "arm/mmu.h"
#include "arm/scu.h"
#include "arm/timer.h"

#include "hw/codec.h"
#include "hw/gpulcd.h"
#include "hw/i2c.h"
#include "hw/mcu.h"
#include <spi.h>

#include "system/sections.h"

#define CFG11_MPCORE_CLKCNT	((vu16*)(0x10141300))
#define CFG11_SOCINFO		((vu16*)(0x10140FFC))

#define LEGACY_BOOT_ENTRYPOINT	((vu32*)0x1FFFFFFC)
#define LEGACY_BOOT_ROUTINE_SMP	(0x0001004C)

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
		*CFG11_MPCORE_CLKCNT = 0x8001;
		do {
			ARM_WFI();
		} while(!(*CFG11_MPCORE_CLKCNT & 0x8000));
		GIC_Disable(88, BIT(0));
	}
}

void SYS_CoreZeroInit(void)
{
	GIC_GlobalReset();

	*LEGACY_BOOT_ENTRYPOINT = 0;

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
	if (SYS_IsNewConsole()) {
		MMU_Map(0x20000000, 0x20000000, 256UL << 20, MMU_FLAGS(CACHED_WB, READ_WRITE, 1, 1));
	} else {
		MMU_Map(0x20000000, 0x20000000, 128UL << 20, MMU_FLAGS(CACHED_WB, READ_WRITE, 1, 1));
	}

	// Initialize peripherals
	PXI_Reset();

	I2C_init();
	MCU_Init();

	SPI_Init();
	CODEC_Init();

	GPU_Init();
	GPU_PSCFill(VRAM_START, VRAM_END, 0);
	GPU_SetFramebuffers((u32[]){VRAM_TOP_LA, VRAM_TOP_LB,
								VRAM_TOP_RA, VRAM_TOP_RB,
								VRAM_BOT_A,  VRAM_BOT_B});

	GPU_SetFramebufferMode(0, PDC_RGB24);
	GPU_SetFramebufferMode(1, PDC_RGB24);

	MCU_PushToLCD(true);
	TIMER_WaitTicks(CLK_MS_TO_TICKS(5));
}

void SYS_CoreInit(void)
{
	// Reset local GIC registers
	GIC_LocalReset();

	// Set up MMU registers
	MMU_Init();

	// Enable fancy ARM11 features
	ARM_SetACR(ARM_GetACR() |
		ACR_RETSTK | ACR_DBPRED | ACR_SBPRED | ACR_FOLDING | ACR_SMP);

	ARM_SetCR(ARM_GetCR() |
		CR_MMU | CR_CACHES | CR_FLOWPRED | CR_HIGHVEC | CR_DSUBPAGE);

	ARM_DSB();

	ARM_EnableInterrupts();
}

void SYS_CoreZeroShutdown(void)
{
	ARM_DisableInterrupts();
	GIC_GlobalReset();
}

void __attribute__((noreturn)) SYS_CoreShutdown(void)
{
	u32 core = ARM_CoreID();

	ARM_DisableInterrupts();

	GIC_LocalReset();

	ARM_WbInvDC();
	ARM_InvIC();
	ARM_DSB();

	ARM_SetCR(ARM_GetCR() & ~(CR_MMU | CR_CACHES | CR_FLOWPRED));
	ARM_SetACR(ARM_GetACR() &
		~(ACR_RETSTK | ACR_DBPRED | ACR_SBPRED | ACR_FOLDING | ACR_SMP));

	SPI_Deinit();

	if (!core) {
		while(*LEGACY_BOOT_ENTRYPOINT == 0);
		((void (*)(void))(*LEGACY_BOOT_ENTRYPOINT))();
	} else {
		// Branch to bootrom function that does SMP reinit magic
		// (waits for IPI + branches to word @ 0x1FFFFFDC)
		((void (*)(void))LEGACY_BOOT_ROUTINE_SMP)();
	}
	__builtin_unreachable();
}
