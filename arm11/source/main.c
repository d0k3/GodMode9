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

#include <common.h>
#include <types.h>
#include <shmem.h>
#include <arm.h>
#include <pxi.h>

#include "arm/gic.h"

#include "hw/hid.h"
#include "hw/gpulcd.h"
#include "hw/i2c.h"
#include "hw/mcu.h"
#include "hw/nvram.h"

#include "system/sys.h"
#include "system/event.h"

static const u8 brightness_lvls[] = {
	0x10, 0x17, 0x1E, 0x25,
	0x2C, 0x34, 0x3C, 0x44,
	0x4D, 0x56, 0x60, 0x6B,
	0x79, 0x8C, 0xA7, 0xD2
};

#ifndef FIXED_BRIGHTNESS
static int prev_bright_lvl;
static bool auto_brightness;
#endif

static SystemSHMEM __attribute__((section(".shared"))) sharedMem;

static void vblankUpdate(void)
{
	if (!getEventIRQ()->test(VBLANK_INTERRUPT, true))
		return;

	#ifndef FIXED_BRIGHTNESS
	int cur_bright_lvl = (mcuGetVolumeSlider() >> 2) % countof(brightness_lvls);
	if ((cur_bright_lvl != prev_bright_lvl) && auto_brightness) {
		prev_bright_lvl = cur_bright_lvl;
		u8 br = brightness_lvls[cur_bright_lvl];
		GFX_setBrightness(br, br);
	}
	#endif

	// handle shell events
	static const u32 mcuEvShell = MCUEV_HID_SHELL_OPEN | MCUEV_HID_SHELL_CLOSE;
	u32 shell = getEventMCU()->test(mcuEvShell, mcuEvShell);
	if (shell & MCUEV_HID_SHELL_CLOSE) {
		GFX_powerOffBacklights(GFX_BLIGHT_BOTH);
	} else if (shell & MCUEV_HID_SHELL_OPEN) {
		GFX_powerOnBacklights(GFX_BLIGHT_BOTH);
	}

	sharedMem.hidState.full = HID_GetState();
}

static u32 pxiRxUpdate(u32 *args)
{
	u32 msg, lo, hi;

	if (!getEventIRQ()->test(PXI_RX_INTERRUPT, true))
		return PXICMD_NONE;

	msg = PXI_Recv();
	lo = msg & 0xFFFF;
	hi = msg >> 16;

	PXI_RecvArray(args, hi);
	return lo;
}

void __attribute__((noreturn)) MainLoop(void)
{
	bool runPxiCmdProcessor = true;

	#ifdef FIXED_BRIGHTNESS
	u8 fixed_bright_lvl = brightness_lvls[clamp(FIXED_BRIGHTNESS, 0, countof(brightness_lvls)-1)];
	GFX_setBrightness(fixed_bright_lvl, fixed_bright_lvl);
	#else
	prev_bright_lvl = -1;
	auto_brightness = true;
	#endif

	// initialize state stuff
	getEventIRQ()->reset();
	getEventMCU()->reset();
	memset(&sharedMem, 0, sizeof(sharedMem));

	// configure interrupts
	gicSetInterruptConfig(PXI_RX_INTERRUPT, BIT(0), GIC_PRIO0, NULL);
	gicSetInterruptConfig(VBLANK_INTERRUPT, BIT(0), GIC_PRIO0, NULL);
	gicSetInterruptConfig(MCU_INTERRUPT, BIT(0), GIC_PRIO0, NULL);

	// enable interrupts
	gicEnableInterrupt(MCU_INTERRUPT);

	// perform gpu init after initializing mcu but before
	// enabling the pxi system and the vblank handler
	GFX_init(GFX_RGB565);

	gicEnableInterrupt(PXI_RX_INTERRUPT);
	gicEnableInterrupt(VBLANK_INTERRUPT);

	// ARM9 won't try anything funny until this point
	PXI_Barrier(PXI_BOOT_BARRIER);

	// Process commands until the ARM9 tells
	// us it's time to boot something else
	// also handles VBlank events as needed
	do {
		u32 pxiCmd, pxiReply, args[PXI_MAX_ARGS];

		vblankUpdate();
		pxiCmd = pxiRxUpdate(args);

		switch(pxiCmd) {
			// ignore args and just wait until the next event
			case PXICMD_NONE:
				ARM_WFI();
				break;

			// revert to legacy boot mode
			case PXICMD_LEGACY_BOOT:
				runPxiCmdProcessor = false;
				pxiReply = 0;
				break;

			// returns the shared memory address
			case PXICMD_GET_SHMEM_ADDRESS:
				pxiReply = (u32)&sharedMem;
				break;

			// takes in a single argument word and performs either an
			// I2C read or write depending on the value of the top bit
			case PXICMD_I2C_OP:
			{
				u32 devId, regAddr, size;

				devId = (args[0] & 0xff);
				regAddr = (args[0] >> 8) & 0xFF;
				size = (args[0] >> 16) % SHMEM_BUFFER_SIZE;

				if (args[0] & BIT(31)) {
					pxiReply = I2C_writeRegBuf(devId, regAddr, sharedMem.dataBuffer.b, size);
				} else {
					pxiReply = I2C_readRegBuf(devId, regAddr, sharedMem.dataBuffer.b, size);
				}
				break;
			}

			// checks whether the NVRAM chip is online (not doing any work)
			case PXICMD_NVRAM_ONLINE:
				pxiReply = (NVRAM_Status() & NVRAM_SR_WIP) == 0;
				break;

			// reads data from the NVRAM chip
			case PXICMD_NVRAM_READ:
				NVRAM_Read(args[0], sharedMem.dataBuffer.w, args[1]);
				pxiReply = 0;
				break;

			// sets the notification LED with the given color and period
			case PXICMD_SET_NOTIFY_LED:
				mcuSetStatusLED(args[0], args[1]);
				pxiReply = 0;
				break;

			// sets the LCDs brightness (if FIXED_BRIGHTNESS is disabled)
			case PXICMD_SET_BRIGHTNESS:
			{
				pxiReply = GFX_getBrightness();
				#ifndef FIXED_BRIGHTNESS
				s32 newbrightness = (s32)args[0];
				if ((newbrightness > 0) && (newbrightness < 0x100)) {
					GFX_setBrightness(newbrightness, newbrightness);
					auto_brightness = false;
				} else {
					prev_bright_lvl = -1;
					auto_brightness = true;
				}
				#endif
				break;
			}

			// replies -1 on default
			default:
				pxiReply = 0xFFFFFFFF;
				break;
		}

		if (pxiCmd != PXICMD_NONE)
			PXI_Send(pxiReply); // was a command sent from the ARM9, send a response
	} while(runPxiCmdProcessor);

	// perform deinit in reverse order
	gicDisableInterrupt(VBLANK_INTERRUPT);
	gicDisableInterrupt(PXI_RX_INTERRUPT);

	// unconditionally reinitialize the screens
	// in RGB24 framebuffer mode
	GFX_init(GFX_BGR8);

	gicDisableInterrupt(MCU_INTERRUPT);

	// Wait for the ARM9 to do its firmlaunch setup
	PXI_Barrier(PXI_FIRMLAUNCH_BARRIER);

	SYS_CoreZeroShutdown();
	SYS_CoreShutdown();
}
