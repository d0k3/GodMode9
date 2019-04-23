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

typedef void (*IRQ_Handler)(u32 irqn);

#define GIC_SOFTIRQ_SOURCE(n)	(((n) >> 10) & 0xF)
#define GIC_SOFTIRQ_NUMBER(n)	((n) & 0x3FF)

enum {
	GIC_SOFTIRQ_NORMAL = 0,
	GIC_SOFTIRQ_NOTSELF = 1,
	GIC_SOFTIRQ_SELF = 2
};

enum {
	GIC_HIGHEST_PRIO = 0x0,
	GIC_LOWEST_PRIO = 0xE,
};

void GIC_GlobalReset(void);
void GIC_LocalReset(void);

int GIC_Enable(u32 irqn, u32 coremask, u32 prio, IRQ_Handler handler);
int GIC_Disable(u32 irqn, u32 coremask);

void GIC_TriggerSoftIRQ(u32 irqn, u32 mode, u32 coremask);
