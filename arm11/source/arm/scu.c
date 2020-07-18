/*
 *   This file is part of GodMode9
 *   Copyright (C) 2018-2019 Wolfvak
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

#define REG_SCU_CNT	(*REG_ARM_PMR(0x00, u32))
#define REG_SCU_CFG	(*REG_ARM_PMR(0x04, u32))
#define REG_SCU_CPU	(*REG_ARM_PMR(0x08, u32))
#define REG_SCU_INV	(*REG_ARM_PMR(0x0C, u32))

void SCU_Init(void)
{
	REG_SCU_CNT = 0x1FFE;
	REG_SCU_INV = 0xFFFF;
	REG_SCU_CNT = 0x3FFF;
}
