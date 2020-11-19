/*
 *   This file is part of GodMode9
 *   Copyright (C) 2017-2020 Wolfvak
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

#include <common.h>
#include <arm.h>

typedef void (*gicIrqHandler)(u32 irqn);

enum {
	GIC_LEVELHIGH_NN = 0, // no interrupts use level high triggers so far
	GIC_LEVELHIGH_1N = 1,

	GIC_RISINGEDGE_NN = 2,
	GIC_RISINGEDGE_1N = 3

	// With the 1-N model, an interrupt that is taken on any CPU clears the Pending
	// status on all CPUs.
	// With the N-N model, all CPUs receive the interrupt independently. The Pending
	// status is cleared only for the CPU that takes it, not for the other CPUs
};

enum {
	GIC_PRIO0 = 0x00,
	GIC_PRIO1 = 0x10,
	GIC_PRIO2 = 0x20,
	GIC_PRIO3 = 0x30,
	GIC_PRIO4 = 0x40,
	GIC_PRIO5 = 0x50,
	GIC_PRIO6 = 0x60,
	GIC_PRIO7 = 0x70,
	GIC_PRIO14 = 0xE0,
	GIC_PRIO15 = 0xF0,
};

#define GIC_PRIO_HIGHEST GIC_PRIO0
#define GIC_PRIO_LOWEST GIC_PRIO14
#define GIC_PRIO_NEVER GIC_PRIO15

void gicGlobalReset(void);
void gicLocalReset(void);

/*
 Notes from https://static.docs.arm.com/ddi0360/f/DDI0360F_arm11_mpcore_r2p0_trm.pdf

 INTERRUPT ENABLE:
 Interrupts 0-15 fields are read as one, that is, always enabled, and write to these fields
 have no effect.
 Notpresent interrupts (depending on the Interrupt Controller Type Register and
 interrupt number field) related fields are read as zero and writes to these fields have no
 effect.

 INTERRUPT PRIORITY:
 The first four registers are aliased for each MP11 CPU, that is, the priority set for
 ID0-15 and ID29-31 can be different for each MP11 CPU. The priority of IPIs ID0-15
 depends on the receiving CPU, not the sending CPU.

 INTERRUPT CPU TARGET:
 For MP11 CPU n, CPU targets 29, 30 and 31 are read as (1 << n). Writes are ignored.
 For IT0-IT28, these fields are read as zero and writes are ignored.

 INTERRUPT CONFIGURATION:
 For ID0-ID15, bit 1 of the configuration pair is always read as one, that is, rising edge
 sensitive.
 For ID0-ID15, bit 0 (software model) can be configured and applies to the interrupts
 sent from the writing MP11 CPU.
 For ID29, and ID30, the configuration pair is always read as b10, that is rising edge
 sensitive and N-N software model because these IDs are allocated to timer and
 watchdog interrupts that are CPU-specific
*/

#define COREMASK_ALL	(BIT(MAX_CPU) - 1)

void gicSetInterruptConfig(u32 irqn, u32 coremask, u32 prio, gicIrqHandler handler);
void gicClearInterruptConfig(u32 irqn);

void gicEnableInterrupt(u32 irqn);
void gicDisableInterrupt(u32 irqn);

enum {
	GIC_SOFTIRQ_LIST = 0,
	GIC_SOFTIRQ_OTHERS = 1, // all except self
	GIC_SOFTIRQ_SELF = 2,
};

#define GIC_SOFTIRQ_SOURCE(n)	(((n) >> 10) & 0xF)
#define GIC_SOFTIRQ_ID(n)	((n) & 0x3FF)

#define GIC_SOFTIRQ_FMT(id, filter, coremask) \
	((id) | ((coremask) << 16) | ((filter) << 24))
	// id & 0xf, coremask & 3, filter & 3
	// coremask is only used with filter == GIC_SOFTIRQ_LIST

#define GIC_SOFTIRQ_SRC(x)	(((x) >> 10) % MAX_CPU)

void gicTriggerSoftInterrupt(u32 softirq);
