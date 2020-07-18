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
#include <arm.h>

#include <vram.h>

static const u8 num_font[16*8];

#define SCREEN ((u16*)(VRAM_TOP_LA))

void draw_char(u16 *fb, int c, int x, int y)
{
	for (int _y = 0; _y < 8; _y++) {
		for (int _x = 0; _x < 8; _x++) {
			u16 *fbpos = fb + (240 - (y + _y)) + (240 * (x + _x));

			u8 mask = (num_font[(c * 8) + _y] >> (8 - _x)) & 1;

			if (mask)
				*fbpos = ~0;
			else
				*fbpos = 0;
		}
	}
}

void draw_hex(u16 *fb, u32 num, int x, int y)
{
	x += 7*8;
	for (int i = 0; i < 8; i++) {
		draw_char(fb, num & 0xf, x, y);
		num >>= 4;
		x -= 8;
	}
}

void do_exception(u32 type, u32 *regs)
{
	for (int i = 0; i < 400*240; i++)
		SCREEN[i] = 0;

	draw_hex(SCREEN, type, 8, 16);

	for (int i = 0; i < 20; i += 2) {
		draw_hex(SCREEN, i, 8, 32 + (i * 4));
		draw_hex(SCREEN, regs[i], 80, 32 + (i * 4));

		draw_hex(SCREEN, i + 1, 208, 32 + (i * 4));
		draw_hex(SCREEN, regs[i + 1], 280, 32 + (i * 4));
	}

	while(1)
		ARM_WFI();
}

static const u8 num_font[] = {
	0b00000000,
	0b00011000,
	0b00100100,
	0b00101100,
	0b00110100,
	0b00100100,
	0b00011000,
	0b00000000, // 0

	0b00000000,
	0b00011000,
	0b00101000,
	0b00001000,
	0b00001000,
	0b00001000,
	0b00111100,
	0b00000000, // 1

	0b00000000,
	0b00011000,
	0b00100100,
	0b00000100,
	0b00001000,
	0b00010000,
	0b00111100,
	0b00000000, // 2

	0b00000000,
	0b00111000,
	0b00000100,
	0b00011000,
	0b00000100,
	0b00000100,
	0b00111000,
	0b00000000, // 3

	0b00000000,
	0b00100100,
	0b00100100,
	0b00111100,
	0b00000100,
	0b00000100,
	0b00000100,
	0b00000000, // 4

	0b00000000,
	0b00111100,
	0b00100000,
	0b00111000,
	0b00000100,
	0b00000100,
	0b00111000,
	0b00000000, // 5

	0b00000000,
	0b00011100,
	0b00100000,
	0b00111000,
	0b00100100,
	0b00100100,
	0b00011000,
	0b00000000, // 6

	0b00000000,
	0b00111100,
	0b00000100,
	0b00000100,
	0b00001000,
	0b00010000,
	0b00010000,
	0b00000000, // 7

	0b00000000,
	0b00011000,
	0b00100100,
	0b00011000,
	0b00100100,
	0b00100100,
	0b00011000,
	0b00000000, // 8

	0b00000000,
	0b00011000,
	0b00100100,
	0b00011100,
	0b00000100,
	0b00000100,
	0b00111000,
	0b00000000, // 9

	0b00000000,
	0b00011000,
	0b00100100,
	0b00111100,
	0b00100100,
	0b00100100,
	0b00100100,
	0b00000000, // A

	0b00000000,
	0b00111000,
	0b00100100,
	0b00111000,
	0b00100100,
	0b00100100,
	0b00111000,
	0b00000000, // B

	0b00000000,
	0b00011100,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00011100,
	0b00000000, // C

	0b00000000,
	0b00110000,
	0b00101000,
	0b00100100,
	0b00100100,
	0b00101000,
	0b00110000,
	0b00000000, // C

	0b00000000,
	0b00111100,
	0b00100000,
	0b00111100,
	0b00100000,
	0b00100000,
	0b00111100,
	0b00000000, // E

	0b00000000,
	0b00111100,
	0b00100000,
	0b00111100,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00000000, // F
};
