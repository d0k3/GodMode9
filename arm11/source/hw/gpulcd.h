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

#pragma once
#include <types.h>

#define VBLANK_INTERRUPT	(0x2A)

void LCD_SetBrightness(u8 brightness);
u8 LCD_GetBrightness(void);

void LCD_Deinitialize(void);

void GPU_PSCFill(u32 start, u32 end, u32 fv);

enum {
	PDC_RGBA8 = 0,
	PDC_RGB24 = 1,
	PDC_RGB565 = 2,
	PDC_RGB5A1 = 3,
	PDC_RGBA4 = 4,
};

void GPU_SetFramebufferMode(u32 screen, u8 mode);
void GPU_SetFramebuffers(const u32 *framebuffers);
void GPU_Init(void);
