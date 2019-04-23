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

#include "arm/gic.h"
#include "arm/timer.h"

#define TIMER_INTERRUPT	(0x1E)

#define REG_TIMER(c, n)	REG_ARM_PMR(0x700 + ((c) * 0x100) + (n), u32)
#define TIMER_THIS_CPU	(-1)

#define REG_TIMER_LOAD(c)	*REG_TIMER((c), 0x00)
#define REG_TIMER_COUNT(c)	*REG_TIMER((c), 0x04)
#define REG_TIMER_CNT(c)	*REG_TIMER((c), 0x08)
#define REG_TIMER_IRQ(c)	*REG_TIMER((c), 0x0C)

#define TIMER_CNT_SCALE(n)	((n) << 8)
#define TIMER_CNT_INT_EN	BIT(2)
#define TIMER_CNT_RELOAD	BIT(1)
#define TIMER_CNT_ENABLE	BIT(0)

void TIMER_WaitTicks(u32 ticks)
{
	REG_TIMER_IRQ(TIMER_THIS_CPU) = 1;
	REG_TIMER_CNT(TIMER_THIS_CPU) = 0;
	REG_TIMER_LOAD(TIMER_THIS_CPU) = ticks;
	REG_TIMER_CNT(TIMER_THIS_CPU) = TIMER_CNT_ENABLE;
	while(REG_TIMER_COUNT(TIMER_THIS_CPU));
}
