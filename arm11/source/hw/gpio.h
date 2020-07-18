/*
 *   This file is part of GodMode9
 *   Copyright (C) 2017 derrek, profi200
 *   Copyright (C) 2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
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

#define REG_GPIO   ((vu16*)(0x10100000 + 0x47000))

static inline void GPIO_setBit(u16 reg, u8 bitNum)
{
	REG_GPIO[reg] |= 1u<<bitNum;
}

static inline void GPIO_clearBit(u16 reg, u8 bitNum)
{
	REG_GPIO[reg] &= ~(1u<<bitNum);
}
