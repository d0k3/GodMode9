/*
 *   This file is part of GodMode9
 *   Copyright (C) 2017-2019 Wolfvak
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
#include <vram.h>
#include <arm.h>

#include "arm/timer.h"

#include "hw/i2c.h"
#include "hw/mcu.h"
#include "hw/gpulcd.h"

#include "system/event.h"

static struct
{
	u16 lcdIds;            // Bits 0-7 top screen, 8-15 bottom screen.
	bool lcdIdsRead;
	u8 lcdPower;           // 1 = on. Bit 4 top light, bit 2 bottom light, bit 0 LCDs.
	u8 lcdLights[2];       // LCD backlight brightness. Top, bottom.
	u32 framebufs[2];      // For each screen
	u8 doubleBuf[2];       // Top, bottom, 1 = enable.
	u16 strides[2];        // Top, bottom
	u32 formats[2];        // Top, bottom
} g_gfxState = {0};

static void setupDisplayController(u8 lcd);
static void resetLcdsMaybe(void);
static void waitLcdsReady(void);

static u32 gxModeWidth(unsigned c) {
	switch(c) {
		case 0: return 4;
		case 1: return 3;
		default: return 2;
	}
}

unsigned GFX_init(GfxFbFmt mode)
{
	unsigned err = 0;

	REG_CFG11_GPUPROT = 0;

	// Reset
	REG_PDN_GPU_CNT = PDN_GPU_CNT_CLK_E;
	ARM_WaitCycles(12);
	REG_PDN_GPU_CNT = PDN_GPU_CNT_CLK_E | PDN_GPU_CNT_RST_ALL;
	REG_GX_GPU_CLK = 0x100;
	REG_GX_PSC_VRAM = 0;
	REG_GX_PSC_FILL0_CNT = 0;
	REG_GX_PSC_FILL1_CNT = 0;
	REG_GX_PPF_CNT = 0;

	// LCD framebuffer setup.

	g_gfxState.strides[0] = 240 * gxModeWidth(mode);
	g_gfxState.strides[1] = 240 * gxModeWidth(mode);

	g_gfxState.framebufs[0] = VRAM_TOP_LA;
	g_gfxState.framebufs[1] = VRAM_BOT_A;

	g_gfxState.formats[0] = mode | BIT(6) | BIT(9);
	g_gfxState.formats[1] = mode | BIT(9);

	setupDisplayController(0);
	setupDisplayController(1);
	REG_LCD_PDC0_SWAP = 0; // Select framebuf 0.
	REG_LCD_PDC1_SWAP = 0;
	REG_LCD_PDC0_CNT = PDC_CNT_OUT_E | PDC_CNT_I_MASK_ERR | PDC_CNT_I_MASK_H | PDC_CNT_E; // Start
	REG_LCD_PDC1_CNT = PDC_CNT_OUT_E | PDC_CNT_I_MASK_ERR | PDC_CNT_I_MASK_H | PDC_CNT_E;

	// LCD reg setup.
	REG_LCD_ABL0_FILL = 1u<<24; // Force blackscreen
	REG_LCD_ABL1_FILL = 1u<<24; // Force blackscreen
	REG_LCD_PARALLAX_CNT = 0;
	REG_LCD_PARALLAX_PWM = 0xA390A39;
	REG_LCD_RST = 0;
	REG_LCD_UNK00C = 0x10001;

	// Clear used VRAM
	REG_GX_PSC_FILL0_S_ADDR = VRAM_TOP_LA >> 3;
	REG_GX_PSC_FILL0_E_ADDR = VRAM_END >> 3;
	REG_GX_PSC_FILL0_VAL = 0;
	REG_GX_PSC_FILL0_CNT = BIT(9) | BIT(0);

	// Backlight and other stuff.
	REG_LCD_ABL0_LIGHT = 0;
	REG_LCD_ABL0_CNT = 0;
	REG_LCD_ABL0_LIGHT_PWM = 0;
	REG_LCD_ABL1_LIGHT = 0;
	REG_LCD_ABL1_CNT = 0;
	REG_LCD_ABL1_LIGHT_PWM = 0;

	REG_LCD_RST = 1;
	REG_LCD_UNK00C = 0;
	TIMER_WaitMS(10);
	resetLcdsMaybe();
	MCU_controlLCDPower(2u); // Power on LCDs.
	if(eventWait(getEventMCU(), 0x3Fu<<24, 0x3Fu<<24) != 2u<<24) __builtin_trap();

	waitLcdsReady();
	REG_LCD_ABL0_LIGHT_PWM = 0x1023E;
	REG_LCD_ABL1_LIGHT_PWM = 0x1023E;
	MCU_controlLCDPower(0x28u); // Power on backlights.
	if(eventWait(getEventMCU(), 0x3Fu<<24, 0x3Fu<<24) != 0x28u<<24) __builtin_trap();
	g_gfxState.lcdPower = 0x15; // All on.

	// Make sure the fills finished.
	REG_LCD_ABL0_FILL = 0;
	REG_LCD_ABL1_FILL = 0;

	// GPU stuff.
	REG_GX_GPU_CLK = 0x70100;
	*((vu32*)0x10400050) = 0x22221200;
	*((vu32*)0x10400054) = 0xFF2;

	GFX_setBrightness(0x80, 0x80);

	return err;
}

static u16 getLcdIds(void)
{
	u16 ids;

	if(!g_gfxState.lcdIdsRead)
	{
		g_gfxState.lcdIdsRead = true;

		u16 top, bot;
		I2C_writeReg(I2C_DEV_LCD0, 0x40, 0xFF);
		I2C_readRegBuf(I2C_DEV_LCD0, 0x40, (u8*)&top, 2);
		I2C_writeReg(I2C_DEV_LCD1, 0x40, 0xFF);
		I2C_readRegBuf(I2C_DEV_LCD1, 0x40, (u8*)&bot, 2);

		ids = top>>8;
		ids |= bot & 0xFF00u;
		g_gfxState.lcdIds = ids;
	}
	else ids = g_gfxState.lcdIds;

	return ids;
}

static void resetLcdsMaybe(void)
{
	const u16 ids = getLcdIds();

	// Top screen
	if(ids & 0xFFu) I2C_writeReg(I2C_DEV_LCD0, 0xFE, 0xAA);
	else
	{
		I2C_writeReg(I2C_DEV_LCD0, 0x11, 0x10);
		I2C_writeReg(I2C_DEV_LCD0, 0x50, 1);
	}

	// Bottom screen
	if(ids>>8) I2C_writeReg(I2C_DEV_LCD1, 0xFE, 0xAA);
	else       I2C_writeReg(I2C_DEV_LCD1, 0x11, 0x10);

	I2C_writeReg(I2C_DEV_LCD0, 0x60, 0);
	I2C_writeReg(I2C_DEV_LCD1, 0x60, 0);
	I2C_writeReg(I2C_DEV_LCD0, 1, 0x10);
	I2C_writeReg(I2C_DEV_LCD1, 1, 0x10);
}

static void waitLcdsReady(void)
{
	const u16 ids = getLcdIds();

	if((ids & 0xFFu) == 0 || (ids>>8) == 0) // Unknown LCD?
	{
		TIMER_WaitMS(150);
	}
	else
	{
		u32 i = 0;
		do
		{
			u16 top, bot;
			I2C_writeReg(I2C_DEV_LCD0, 0x40, 0x62);
			I2C_readRegBuf(I2C_DEV_LCD0, 0x40, (u8*)&top, 2);
			I2C_writeReg(I2C_DEV_LCD1, 0x40, 0x62);
			I2C_readRegBuf(I2C_DEV_LCD1, 0x40, (u8*)&bot, 2);

			if((top>>8) == 1 && (bot>>8) == 1) break;

			TIMER_WaitMS(33);
		} while(i++ < 10);
	}
}

void GFX_powerOnBacklights(GfxBlight mask)
{
	g_gfxState.lcdPower |= mask;

	mask <<= 1;
	MCU_controlLCDPower(mask); // Power on backlights.
	eventWait(getEventMCU(), 0x3F<<24, 0x3F<<24);
	/*if(mcuEventWait(0x3Fu<<24) != (u32)mask<<24)
		__builtin_trap();*/
}

void GFX_powerOffBacklights(GfxBlight mask)
{
	g_gfxState.lcdPower &= ~mask;

	MCU_controlLCDPower(mask); // Power off backlights.
	eventWait(getEventMCU(), 0x3F<<24, 0x3F<<24);
	/*if(mcuEventWait(0x3Fu<<24) != (u32)mask<<24)
		__builtin_trap();*/
}

u8 GFX_getBrightness(void)
{
	return REG_LCD_ABL0_LIGHT;
}

void GFX_setBrightness(u8 top, u8 bot)
{
	g_gfxState.lcdLights[0] = top;
	g_gfxState.lcdLights[1] = bot;
	REG_LCD_ABL0_LIGHT = top;
	REG_LCD_ABL1_LIGHT = bot;
}

void GFX_setForceBlack(bool top, bool bot)
{
	REG_LCD_ABL0_FILL = (u32)top<<24; // Force blackscreen
	REG_LCD_ABL1_FILL = (u32)bot<<24; // Force blackscreen
}

static void setupDisplayController(u8 lcd)
{
	if(lcd > 1) return;

	static const u32 displayCfgs[2][24] =
	{
		{
			// PDC0 regs 0-0x4C.
			450, 209, 449, 449, 0, 207, 209, 453<<16 | 449,
			1<<16 | 0, 413, 2, 402, 402, 402, 1, 2,
			406<<16 | 402, 0, 0<<4 | 0, 0<<16 | 0xFF<<8 | 0,
			// PDC0 regs 0x5C-0x64.
			400<<16 | 240, // Width and height.
			449<<16 | 209,
			402<<16 | 2,
			// PDC0 reg 0x9C.
			0<<16 | 0
		},
		{
			// PDC1 regs 0-0x4C.
			450, 209, 449, 449, 205, 207, 209, 453<<16 | 449,
			1<<16 | 0, 413, 82, 402, 402, 79, 80, 82,
			408<<16 | 404, 0, 1<<4 | 1, 0<<16 | 0<<8 | 0xFF,
			// PDC1 regs 0x5C-0x64.
			320<<16 | 240, // Width and height.
			449<<16 | 209,
			402<<16 | 82,
			// PDC1 reg 0x9C.
			0<<16 | 0
		}
	};

	const u32 *const cfg = displayCfgs[lcd];
	vu32 *const regs = (vu32*)(GX_REGS_BASE + 0x400 + (0x100u * lcd));

	for (unsigned i = 0; i < 0x50/4; i++)
		regs[i] = cfg[i];

	for (unsigned i = 0; i < 0xC/4; i++)
		regs[23 + i] = cfg[20 + i];

	regs[36] = g_gfxState.strides[lcd]; // PDC reg 0x90 stride.
	regs[39] = cfg[23];                 // PDC reg 0x9C.

	// PDC regs 0x68, 0x6C, 0x94, 0x98 and 0x70.
	regs[26] = g_gfxState.framebufs[lcd]; // Framebuffer A first address.
	regs[27] = g_gfxState.framebufs[lcd]; // Framebuffer A second address.
	regs[37] = g_gfxState.framebufs[lcd]; // Framebuffer B first address.
	regs[38] = g_gfxState.framebufs[lcd]; // Framebuffer B second address.
	regs[28] = g_gfxState.formats[lcd];   // Format

	regs[32] = 0; // Gamma table index 0.
	for(u32 i = 0; i < 256; i++) regs[33] = 0x10101u * i;
}
