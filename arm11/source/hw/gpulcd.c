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

#include <types.h>
#include <vram.h>

#include "hw/gpulcd.h"

/* LCD Configuration Registers */
#define REG_LCD(x)	((vu32*)(0x10202000 + (x)))
void LCD_SetBrightness(u8 brightness)
{
	*REG_LCD(0x240) = brightness;
	*REG_LCD(0xA40) = brightness;
}

u8 LCD_GetBrightness(void)
{
	return *REG_LCD(0x240);
}

void LCD_Initialize(u8 brightness)
{
	*REG_LCD(0x014) = 0x00000001;
	*REG_LCD(0x00C) &= 0xFFFEFFFE;
	*REG_LCD(0x240) = brightness;
	*REG_LCD(0xA40) = brightness;
	*REG_LCD(0x244) = 0x1023E;
	*REG_LCD(0xA44) = 0x1023E;
}

void LCD_Deinitialize(void)
{
	*REG_LCD(0x244) = 0;
	*REG_LCD(0xA44) = 0;
	*REG_LCD(0x00C) = 0;
	*REG_LCD(0x014) = 0;
}

/* GPU Control Registers */
#define REG_GPU_CNT	((vu32*)(0x10141200))


/* GPU DMA */
#define REG_GPU_PSC(n, x)	((vu32*)(0x10400010 + ((n) * 0x10) + (x)))
#define GPU_PSC_START	(0x00)
#define GPU_PSC_END	(0x04)
#define GPU_PSC_FILLVAL	(0x08)
#define GPU_PSC_CNT	(0x0C)

#define GPUDMA_ADDR(x)	((x) >> 3)
#define PSC_START	(BIT(0))
#define PSC_DONE	(BIT(1))
#define PSC_32BIT	(2 << 8)
#define PSC_24BIT	(1 << 8)
#define PSC_16BIT	(0 << 8)

void GPU_PSCFill(u32 start, u32 end, u32 fv)
{
	u32 mp;
	if (start > end)
		return;

	start = GPUDMA_ADDR(start);
	end   = GPUDMA_ADDR(end);
	mp    = (start + end) / 2;

	*REG_GPU_PSC(0, GPU_PSC_START) = start;
	*REG_GPU_PSC(0, GPU_PSC_END) = mp;
	*REG_GPU_PSC(0, GPU_PSC_FILLVAL) = fv;
	*REG_GPU_PSC(0, GPU_PSC_CNT) = PSC_START | PSC_32BIT;

	*REG_GPU_PSC(1, GPU_PSC_START) = mp;
	*REG_GPU_PSC(1, GPU_PSC_END) = end;
	*REG_GPU_PSC(1, GPU_PSC_FILLVAL) = fv;
	*REG_GPU_PSC(1, GPU_PSC_CNT) = PSC_START | PSC_32BIT;

	while(!((*REG_GPU_PSC(0, GPU_PSC_CNT) | *REG_GPU_PSC(1, GPU_PSC_CNT)) & PSC_DONE));
}

/* GPU Display Registers */
#define GPU_PDC(n, x)	((vu32*)(0x10400400 + ((n) * 0x100) + x))
#define PDC_PARALLAX	(BIT(5))
#define PDC_MAINSCREEN	(BIT(6))
#define PDC_FIXSTRIP	(BIT(7))

void GPU_SetFramebuffers(const u32 *framebuffers)
{
	*GPU_PDC(0, 0x68) = framebuffers[0];
	*GPU_PDC(0, 0x6C) = framebuffers[1];
	*GPU_PDC(0, 0x94) = framebuffers[2];
	*GPU_PDC(0, 0x98) = framebuffers[3];
	*GPU_PDC(1, 0x68) = framebuffers[4];
	*GPU_PDC(1, 0x6C) = framebuffers[5];
	*GPU_PDC(0, 0x78) = 0;
	*GPU_PDC(1, 0x78) = 0;
}

void GPU_SetFramebufferMode(u32 screen, u8 mode)
{
	u32 stride, cfg;
	vu32 *fbcfg_reg, *fbstr_reg;

	mode &= 7;
	screen &= 1;
	cfg = PDC_FIXSTRIP | mode;
	if (screen) {
		fbcfg_reg = GPU_PDC(1, 0x70);
		fbstr_reg = GPU_PDC(1, 0x90);
	} else {
		fbcfg_reg = GPU_PDC(0, 0x70);
		fbstr_reg = GPU_PDC(0, 0x90);
		cfg |= PDC_MAINSCREEN;
	}

	stride = 240;
	switch(mode) {
	case PDC_RGBA8:
		stride *= 4;
		break;
	case PDC_RGB24:
		stride *= 3;
		break;
	default:
		stride *= 2;
		break;
	}

	*fbcfg_reg = cfg;
	*fbstr_reg = stride;
}

void GPU_Init(void)
{
	LCD_Initialize(0x20);

	if (*REG_GPU_CNT != 0x1007F) {
		*REG_GPU_CNT = 0x1007F;
		*GPU_PDC(0, 0x00) = 0x000001C2;
		*GPU_PDC(0, 0x04) = 0x000000D1;
		*GPU_PDC(0, 0x08) = 0x000001C1;
		*GPU_PDC(0, 0x0C) = 0x000001C1;
		*GPU_PDC(0, 0x10) = 0x00000000;
		*GPU_PDC(0, 0x14) = 0x000000CF;
		*GPU_PDC(0, 0x18) = 0x000000D1;
		*GPU_PDC(0, 0x1C) = 0x01C501C1;
		*GPU_PDC(0, 0x20) = 0x00010000;
		*GPU_PDC(0, 0x24) = 0x0000019D;
		*GPU_PDC(0, 0x28) = 0x00000002;
		*GPU_PDC(0, 0x2C) = 0x00000192;
		*GPU_PDC(0, 0x30) = 0x00000192;
		*GPU_PDC(0, 0x34) = 0x00000192;
		*GPU_PDC(0, 0x38) = 0x00000001;
		*GPU_PDC(0, 0x3C) = 0x00000002;
		*GPU_PDC(0, 0x40) = 0x01960192;
		*GPU_PDC(0, 0x44) = 0x00000000;
		*GPU_PDC(0, 0x48) = 0x00000000;
		*GPU_PDC(0, 0x5C) = 0x00F00190;
		*GPU_PDC(0, 0x60) = 0x01C100D1;
		*GPU_PDC(0, 0x64) = 0x01920002;
		*GPU_PDC(0, 0x68) = VRAM_START;
		*GPU_PDC(0, 0x6C) = VRAM_START;
		*GPU_PDC(0, 0x70) = 0x00080340;
		*GPU_PDC(0, 0x74) = 0x00010501;
		*GPU_PDC(0, 0x78) = 0x00000000;
		*GPU_PDC(0, 0x90) = 0x000003C0;
		*GPU_PDC(0, 0x94) = VRAM_START;
		*GPU_PDC(0, 0x98) = VRAM_START;
		*GPU_PDC(0, 0x9C) = 0x00000000;

		for (u32 i = 0; i < 256; i++)
			*GPU_PDC(0, 0x84) = 0x10101 * i;

		*GPU_PDC(1, 0x00) = 0x000001C2;
		*GPU_PDC(1, 0x04) = 0x000000D1;
		*GPU_PDC(1, 0x08) = 0x000001C1;
		*GPU_PDC(1, 0x0C) = 0x000001C1;
		*GPU_PDC(1, 0x10) = 0x000000CD;
		*GPU_PDC(1, 0x14) = 0x000000CF;
		*GPU_PDC(1, 0x18) = 0x000000D1;
		*GPU_PDC(1, 0x1C) = 0x01C501C1;
		*GPU_PDC(1, 0x20) = 0x00010000;
		*GPU_PDC(1, 0x24) = 0x0000019D;
		*GPU_PDC(1, 0x28) = 0x00000052;
		*GPU_PDC(1, 0x2C) = 0x00000192;
		*GPU_PDC(1, 0x30) = 0x00000192;
		*GPU_PDC(1, 0x34) = 0x0000004F;
		*GPU_PDC(1, 0x38) = 0x00000050;
		*GPU_PDC(1, 0x3C) = 0x00000052;
		*GPU_PDC(1, 0x40) = 0x01980194;
		*GPU_PDC(1, 0x44) = 0x00000000;
		*GPU_PDC(1, 0x48) = 0x00000011;
		*GPU_PDC(1, 0x5C) = 0x00F00140;
		*GPU_PDC(1, 0x60) = 0x01C100d1;
		*GPU_PDC(1, 0x64) = 0x01920052;
		*GPU_PDC(1, 0x68) = VRAM_START;
		*GPU_PDC(1, 0x6C) = VRAM_START;
		*GPU_PDC(1, 0x70) = 0x00080300;
		*GPU_PDC(1, 0x74) = 0x00010501;
		*GPU_PDC(1, 0x78) = 0x00000000;
		*GPU_PDC(1, 0x90) = 0x000003C0;
		*GPU_PDC(1, 0x9C) = 0x00000000;

		for (u32 i = 0; i < 256; i++)
			*GPU_PDC(1, 0x84) = 0x10101 * i;
	}
}
