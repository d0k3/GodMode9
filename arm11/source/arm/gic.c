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
#include <arm.h>

#include <stdatomic.h>

#include "arm/gic.h"

#include "system/event.h"

/* Generic Interrupt Controller Registers */
#define REG_GIC(cpu, o, t)  REG_ARM_PMR(0x200 + ((cpu) * 0x100) + (o), t)

#define REG_GIC_CONTROL(c)  (*REG_GIC(c, 0x00, u32))
#define REG_GIC_PRIOMASK(c) (*REG_GIC(c, 0x04, u32))
#define REG_GIC_POI(c)  (*REG_GIC(c, 0x08, u32))
#define REG_GIC_IRQACK(c)   (*REG_GIC(c, 0x0C, u32))
#define REG_GIC_IRQEND(c)   (*REG_GIC(c, 0x10, u32))
#define REG_GIC_LASTPRIO(c) (*REG_GIC(c, 0x14, u32))
#define REG_GIC_PENDING(c)  (*REG_GIC(c, 0x18, u32))

#define GIC_THIS_CPU_ALIAS  (-1)
#define GIC_IRQ_SPURIOUS    (1023)

/* Interrupt Distributor Registers */
#define REG_DIC(off, type)  REG_ARM_PMR(0x1000 + (off), type)

#define REG_DIC_CONTROL (*REG_DIC(0x00, u32))
#define REG_DIC_TYPE    (*REG_DIC(0x04, u32))
#define REG_DIC_SETENABLE   REG_DIC(0x100, u32) // 32 intcfg per reg
#define REG_DIC_CLRENABLE   REG_DIC(0x180, u32)
#define REG_DIC_SETPENDING  REG_DIC(0x200, u32)
#define REG_DIC_CLRPENDING  REG_DIC(0x280, u32)
#define REG_DIC_PRIORITY    REG_DIC(0x400, u8) // 1 intcfg per reg (in upper 4 bits)
#define REG_DIC_TARGETCPU   REG_DIC(0x800, u8) // 1 intcfg per reg
#define REG_DIC_CFGREG  REG_DIC(0xC00, u32) // 16 intcfg per reg
#define REG_DIC_SOFTINT (*REG_DIC(0xF00, u32))

// used only on reset routines
#define REG_DIC_PRIORITY32  REG_DIC(0x400, u32) // 4 intcfg per reg (in upper 4 bits)
#define REG_DIC_TARGETCPU32 REG_DIC(0x800, u32) // 4 intcfg per reg

#define GIC_PRIO_NEVER32 \
    (GIC_PRIO_NEVER | (GIC_PRIO_NEVER << 8) | \
    (GIC_PRIO_NEVER << 16) | (GIC_PRIO_NEVER << 24))
#define GIC_PRIO_HIGH32 \
    (GIC_PRIO_HIGHEST | (GIC_PRIO_HIGHEST << 8) | \
    (GIC_PRIO_HIGHEST << 16) | (GIC_PRIO_HIGHEST << 24))

/* CPU source ID is present in Interrupt Acknowledge register? */
#define IRQN_SRC_MASK   (0x7 << 10)

/* Interrupt Handling */
#define LOCAL_IRQS  (32)
#define DIC_MAX_IRQ (LOCAL_IRQS + MAX_IRQ)

#define COREMASK_VALID(x)   (((x) > 0) && ((x) < BIT(MAX_CPU)))

#define IRQN_IS_VALID(n)    ((n) < DIC_MAX_IRQ)

static gicIrqHandler gicIrqHandlers[DIC_MAX_IRQ];
static _Atomic(u32) gicIrqPending[DIC_MAX_IRQ / 32];

static struct {
    u8 tgt;
    u8 prio;
}  gicIrqConfig[DIC_MAX_IRQ];

// gets used whenever a NULL pointer is passed to gicEnableInterrupt
static void gicDummyHandler(u32 irqn) { (void)irqn; return; }

static const struct {
    u8 low, high, mode;
} gicDefaultIrqCfg[] = {
    { .low = 0x00, .high = 0x1F, .mode = GIC_RISINGEDGE_NN },
    { .low = 0x20, .high = 0x23, .mode = GIC_LEVELHIGH_1N },
    { .low = 0x24, .high = 0x24, .mode = GIC_RISINGEDGE_1N },
    { .low = 0x25, .high = 0x27, .mode = GIC_LEVELHIGH_1N },
    { .low = 0x28, .high = 0x2D, .mode = GIC_RISINGEDGE_1N },
    { .low = 0x30, .high = 0x3B, .mode = GIC_LEVELHIGH_1N },
    { .low = 0x40, .high = 0x4E, .mode = GIC_RISINGEDGE_1N },
    { .low = 0x4F, .high = 0x4F, .mode = GIC_LEVELHIGH_1N },
    { .low = 0x50, .high = 0x57, .mode = GIC_RISINGEDGE_1N },
    { .low = 0x58, .high = 0x58, .mode = GIC_LEVELHIGH_1N },
    { .low = 0x59, .high = 0x75, .mode = GIC_RISINGEDGE_1N },
    { .low = 0x76, .high = 0x77, .mode = GIC_LEVELHIGH_1N },
    { .low = 0x78, .high = 0x78, .mode = GIC_RISINGEDGE_1N },
    { .low = 0x79, .high = 0x7d, .mode = GIC_LEVELHIGH_1N },
};

static u8 gicGetDefaultIrqCfg(u32 irqn) {
    for (unsigned i = 0; i < countof(gicDefaultIrqCfg); i++) {
        if ((irqn >= gicDefaultIrqCfg[i].low) && (irqn <= gicDefaultIrqCfg[i].high))
            return gicDefaultIrqCfg[i].mode;
    }
    // TODO: would it be considerably faster to use bsearch?
    return GIC_RISINGEDGE_1N;
}

void gicTopHandler(void)
{
    while(1) {
        u32 irqn, irqsource, index, mask;

        /**
            If more than one of these CPUs reads the Interrupt Acknowledge Register at the
            same time, they can all acknowledge the same interrupt. The interrupt service
            routine must ensure that only one of them tries to process the interrupt, with the
            others returning after writing the ID to the End of Interrupt Register.
        */
        irqsource = REG_GIC_IRQACK(GIC_THIS_CPU_ALIAS);

        if (irqsource == GIC_IRQ_SPURIOUS) // no further processing is needed
            break;

        irqn = irqsource & ~IRQN_SRC_MASK;

        index = irqn / 32;
        mask = BIT(irqn % 32);
        atomic_fetch_or(&gicIrqPending[index], mask);

        (gicIrqHandlers[irqn])(irqsource);
        // if the id is < 16, the source CPU can be obtained from irqn
        // if the handler isn't set, it'll try to branch to 0 and trigger a prefetch abort

        REG_GIC_IRQEND(GIC_THIS_CPU_ALIAS) = irqsource;
    }
}

void gicGlobalReset(void)
{
    u32 dic_type;
    unsigned gicn, intn;

    dic_type = REG_DIC_TYPE;

    // number of local controllers
    gicn = ((dic_type >> 5) & 3) + 1;

    // number of interrupt lines (up to 224 external + 32 fixed internal per CPU)
    intn = ((dic_type & 7) + 1) * 32;

    // clamp it down to the amount of CPUs designed to handle
    if (gicn > MAX_CPU)
        gicn = MAX_CPU;

    // clear the interrupt handler and config table
    getEventIRQ()->reset();
    memset(gicIrqHandlers, 0, sizeof(gicIrqHandlers));
    memset(gicIrqConfig, 0, sizeof(gicIrqConfig));

    // disable all MP11 GICs
    for (unsigned i = 0; i < gicn; i++)
        REG_GIC_CONTROL(i) = 0;

    // disable the main DIC
    REG_DIC_CONTROL = 0;

    // clear all external interrupts
    for (unsigned i = 1; i < (intn / 32); i++) {
        REG_DIC_CLRENABLE[i] = ~0;
        REG_DIC_CLRPENDING[i] = ~0;
    }

    // reset all external priorities to highest by default
    // clear target processor regs
    for (unsigned i = 4; i < (intn / 4); i++) {
        REG_DIC_PRIORITY32[i] = GIC_PRIO_HIGH32;
        REG_DIC_TARGETCPU32[i] = 0;
    }

    // set all interrupts to active level triggered in N-N model
    for (unsigned i = 16; i < (intn / 16); i++)
        REG_DIC_CFGREG[i] = 0;

    // re enable the main DIC
    REG_DIC_CONTROL = 1;

    for (unsigned i = 0; i < gicn; i++) {
        // compare all priority bits
        REG_GIC_POI(i) = 3;

        // don't mask any interrupt with low priority
        REG_GIC_PRIOMASK(i) = 0xF0;

        // enable all the MP11 GICs
        REG_GIC_CONTROL(i) = 1;
    }
}

void gicLocalReset(void)
{
    u32 irq_s;

    // disable all local interrupts
    REG_DIC_CLRENABLE[0] = ~0;
    REG_DIC_CLRPENDING[0] = ~0;

    for (unsigned i = 0; i < 4; i++) {
        REG_DIC_PRIORITY32[i] = GIC_PRIO_HIGH32;
        // local IRQs are always unmasked by default

        // REG_DIC_TARGETCPU[i] = 0;
        // not needed, always read as corresponding MP11 core
    }

    // ack until it gets a spurious IRQ
    do {
        irq_s = REG_GIC_PENDING(GIC_THIS_CPU_ALIAS);
        REG_GIC_IRQEND(GIC_THIS_CPU_ALIAS) = irq_s;
    } while(irq_s != GIC_IRQ_SPURIOUS);
}

static void gicSetIrqCfg(u32 irqn) {
    u32 smt, cfg;

    smt = irqn & 15;
    cfg = REG_DIC_CFGREG[irqn / 16];
    cfg &= ~(3 << smt);
    cfg |= gicGetDefaultIrqCfg(irqn) << smt;
    REG_DIC_CFGREG[irqn / 16] = cfg;
}

void gicSetInterruptConfig(u32 irqn, u32 coremask, u32 prio, gicIrqHandler handler)
{
    if (handler == NULL) // maybe add runtime ptr checks here too?
        handler = gicDummyHandler;

    gicIrqConfig[irqn].tgt = coremask;
    gicIrqConfig[irqn].prio = prio;
    gicIrqHandlers[irqn] = handler;
}

void gicClearInterruptConfig(u32 irqn)
{
    memset(&gicIrqConfig[irqn], 0, sizeof(gicIrqConfig[irqn]));
    gicIrqHandlers[irqn] = NULL;
}

void gicEnableInterrupt(u32 irqn)
{
    REG_DIC_PRIORITY[irqn] = gicIrqConfig[irqn].prio;
    REG_DIC_TARGETCPU[irqn] = gicIrqConfig[irqn].tgt;
    gicSetIrqCfg(irqn);

    REG_DIC_CLRPENDING[irqn / 32] |= BIT(irqn & 0x1F);
    REG_DIC_SETENABLE[irqn / 32] |= BIT(irqn & 0x1F);
}

void gicDisableInterrupt(u32 irqn)
{
    REG_DIC_CLRENABLE[irqn / 32] |= BIT(irqn & 0x1F);
    REG_DIC_CLRPENDING[irqn / 32] |= BIT(irqn & 0x1F);
}

void gicTriggerSoftInterrupt(u32 softirq)
{
    REG_DIC_SOFTINT = softirq;
}

static void irqEvReset(void) {
    memset(&gicIrqPending, 0, sizeof(gicIrqPending));
}

static u32 irqEvTest(u32 param, u32 clear) {
    u32 index, tstmask, clrmask;

    if (param >= DIC_MAX_IRQ)
        bkpt;
    index = param / 32;
    tstmask = BIT(param % 32);
    clrmask = clear ? tstmask : 0;
    return !!(atomic_fetch_and(&gicIrqPending[index], ~clrmask) & tstmask);
}

static const EventInterface evIRQ = {
    .reset = irqEvReset,
    .test = irqEvTest
};

const EventInterface *getEventIRQ(void) {
    return &evIRQ;
}
