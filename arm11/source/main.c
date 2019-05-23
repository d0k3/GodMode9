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

static GlobalSharedMemory SharedMemory_State;

#ifndef FIXED_BRIGHTNESS
static const u8 brightness_lvls[] = {
	0x10, 0x17, 0x1E, 0x25,
	0x2C, 0x34, 0x3C, 0x44,
	0x4D, 0x56, 0x60, 0x6B,
	0x79, 0x8C, 0xA7, 0xD2
};
static int prev_bright_lvl = -1;
#endif

static bool auto_brightness = true;

void VBlank_Handler(u32 __attribute__((unused)) irqn)
{
	#ifndef FIXED_BRIGHTNESS
	int cur_bright_lvl = (MCU_GetVolumeSlider() >> 2) % countof(brightness_lvls);
	if ((cur_bright_lvl != prev_bright_lvl) && auto_brightness) {
		prev_bright_lvl = cur_bright_lvl;
		LCD_SetBrightness(brightness_lvls[cur_bright_lvl]);
	}
	#endif

	// the state should probably be stored on its own
	// setion without caching enabled, since it must
	// be readable by the ARM9 at all times anyway
	SharedMemory_State.hid_state = HID_GetState();
	ARM_WbDC_Range(&SharedMemory_State, sizeof(SharedMemory_State));
	ARM_DMB();
}

static bool legacy_boot = false;

void PXI_RX_Handler(u32 __attribute__((unused)) irqn)
{
	u32 ret, msg, cmd, argc, args[PXI_MAX_ARGS];

	msg = PXI_Recv();
	cmd = msg & 0xFFFF;
	argc = msg >> 16;

	if (argc >= PXI_MAX_ARGS) {
		PXI_Send(0xFFFFFFFF);
		return;
	}

	PXI_RecvArray(args, argc);

	switch (cmd) {
		case PXI_LEGACY_MODE:
		{
			// TODO: If SMP is enabled, an IPI should be sent here (with a DSB)
			legacy_boot = true;
			ret = 0;
			break;
		}

		case PXI_GET_SHMEM:
		{
			ret = (u32)&SharedMemory_State;
			break;
		}

		case PXI_SET_VMODE:
		{
			int mode = args[0] ? PDC_RGB24 : PDC_RGB565;
			GPU_SetFramebufferMode(0, mode);
			GPU_SetFramebufferMode(1, mode);
			ret = 0;
			break;
		}

		case PXI_I2C_READ:
		{
			ARM_InvDC_Range((void*)args[2], args[3]);
			ret = I2C_readRegBuf(args[0], args[1], (u8*)args[2], args[3]);
			ARM_WbDC_Range((void*)args[2], args[3]);
			ARM_DMB();
			break;
		}

		case PXI_I2C_WRITE:
		{
			ARM_InvDC_Range((void*)args[2], args[3]);
			ARM_DMB();
			ret = I2C_writeRegBuf(args[0], args[1], (u8*)args[2], args[3]);
			break;
		}

		case PXI_NVRAM_ONLINE:
		{
			ret = (NVRAM_Status() & NVRAM_SR_WIP) == 0;
			break;
		}

		case PXI_NVRAM_READ:
		{
			ARM_InvDC_Range((void*)args[1], args[2]);
			NVRAM_Read(args[0], (u32*)args[1], args[2]);
			ARM_WbDC_Range((void*)args[1], args[2]);
			ARM_DMB();
			ret = 0;
			break;
		}

		case PXI_NOTIFY_LED:
		{
			MCU_SetNotificationLED(args[0], args[1]);
			ret = 0;
			break;
		}

		case PXI_BRIGHTNESS:
		{
			ret = LCD_GetBrightness();
			if (args[0] && (args[0] < 0x100)) {
				LCD_SetBrightness(args[0]);
				auto_brightness = false;
			} else {
				auto_brightness = true;
			}
			break;
		}

		/* New CMD template:
		case CMD_ID:
		{
			<var declarations/assignments>
			<execute the command>
			<set the return value>
			break;
		}
		*/

		default:
			ret = 0xFFFFFFFF;
			break;
	}

	PXI_Send(ret);
}

void __attribute__((noreturn)) MainLoop(void)
{
	#ifdef FIXED_BRIGHTNESS
	LCD_SetBrightness(FIXED_BRIGHTNESS);
	#endif

	// enable PXI RX interrupt
	GIC_Enable(PXI_RX_INTERRUPT, BIT(0), GIC_HIGHEST_PRIO + 2, PXI_RX_Handler);

	// enable MCU interrupts
	GIC_Enable(MCU_INTERRUPT, BIT(0), GIC_HIGHEST_PRIO + 1, MCU_HandleInterrupts);

	// set up VBlank interrupt to always have the highest priority
	GIC_Enable(VBLANK_INTERRUPT, BIT(0), GIC_HIGHEST_PRIO, VBlank_Handler);

	// ARM9 won't try anything funny until this point
	PXI_Barrier(ARM11_READY_BARRIER);

	// Process IRQs until the ARM9 tells us it's time to boot something else
	do {
		ARM_WFI();
	} while(!legacy_boot);

	SYS_CoreZeroShutdown();
	SYS_CoreShutdown();
}
