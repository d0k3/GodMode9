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

#ifndef FIXED_BRIGHTNESS
static const u8 brightness_lvls[] = {
	0x10, 0x17, 0x1E, 0x25,
	0x2C, 0x34, 0x3C, 0x44,
	0x4D, 0x56, 0x60, 0x6B,
	0x79, 0x8C, 0xA7, 0xD2
};
static int prev_bright_lvl;
static bool auto_brightness;
#endif

static SystemSHMEM __attribute__((section(".shared"))) SharedMemoryState;

void VBlank_Handler(u32 __attribute__((unused)) irqn)
{
	#ifndef FIXED_BRIGHTNESS
	int cur_bright_lvl = (MCU_GetVolumeSlider() >> 2) % countof(brightness_lvls);
	if ((cur_bright_lvl != prev_bright_lvl) && auto_brightness) {
		prev_bright_lvl = cur_bright_lvl;
		u8 br = brightness_lvls[cur_bright_lvl];
		GFX_setBrightness(br, br);
	}
	#endif

	SharedMemoryState.hidState.full = HID_GetState();
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
			ret = (u32)&SharedMemoryState;
			break;
		}

		case PXI_SET_VMODE:
		{
			GFX_init(args[0] ? GFX_BGR8 : GFX_RGB565);
			ret = 0;
			break;
		}

		case PXI_I2C_READ:
		{
			u32 devId, regAddr, size;

			devId = (args[0] & 0xff);
			regAddr = (args[0] >> 8) & 0xff;
			size = (args[0] >> 16) % I2C_SHARED_BUFSZ;

			ret = I2C_readRegBuf(devId, regAddr, SharedMemoryState.i2cBuffer, size);
			break;
		}

		case PXI_I2C_WRITE:
		{
			u32 devId, regAddr, size;

			devId = (args[0] & 0xff);
			regAddr = (args[0] >> 8) & 0xff;
			size = (args[0] >> 16) % I2C_SHARED_BUFSZ;

			ret = I2C_writeRegBuf(devId, regAddr, SharedMemoryState.i2cBuffer, size);
			break;
		}

		case PXI_NVRAM_ONLINE:
		{
			ret = (NVRAM_Status() & NVRAM_SR_WIP) == 0;
			break;
		}

		case PXI_NVRAM_READ:
		{
			NVRAM_Read(args[0], (u32*)SharedMemoryState.spiBuffer, args[1]);
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
			s32 newbrightness = (s32)args[0];
			ret = GFX_getBrightness();
			#ifndef FIXED_BRIGHTNESS
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
	#else
	prev_bright_lvl = -1;
	auto_brightness = true;
	#endif

	// configure interrupts
	gicSetInterruptConfig(PXI_RX_INTERRUPT, BIT(0), GIC_PRIO2, GIC_RISINGEDGE_1N, PXI_RX_Handler);
	gicSetInterruptConfig(MCU_INTERRUPT, BIT(0), GIC_PRIO1, GIC_RISINGEDGE_1N, MCU_HandleInterrupts);
	gicSetInterruptConfig(VBLANK_INTERRUPT, BIT(0), GIC_PRIO0, GIC_RISINGEDGE_1N, VBlank_Handler);

	// enable interrupts
	gicEnableInterrupt(PXI_RX_INTERRUPT);
	gicEnableInterrupt(MCU_INTERRUPT);
	gicEnableInterrupt(VBLANK_INTERRUPT);

	// ARM9 won't try anything funny until this point
	PXI_Barrier(ARM11_READY_BARRIER);

	// Process IRQs until the ARM9 tells us it's time to boot something else
	do {
		ARM_WFI();
	} while(!legacy_boot);

	SYS_CoreZeroShutdown();
	SYS_CoreShutdown();
}
