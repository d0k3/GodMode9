/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#include <types.h>
#include <cpu.h>
#include <gic.h>

#define IRQ_BASE ((vu32*)0x1FFFFFA0)

irq_handler handler_table[MAX_IRQ];
extern void (*main_irq_handler)(void);

irq_handler GIC_AckIRQ(void)
{
    u32 xrq = *GIC_IRQACK;
    irq_handler ret = NULL;
    if (xrq < MAX_IRQ && handler_table[xrq]) {
        ret = handler_table[xrq];
        *GIC_IRQEND = xrq;
    }
    return ret;
}

void GIC_Configure(u32 irq_id, irq_handler hndl)
{
    handler_table[irq_id] = hndl;
    DIC_CLRENABLE[irq_id/32] |= BIT(irq_id & 0x1F);
    DIC_SETENABLE[irq_id/32] |= BIT(irq_id & 0x1F);
    DIC_PROCTGT[irq_id] = 1;
    return;
}

void GIC_Reset(void)
{
    *DIC_CONTROL = 0;
    *GIC_CONTROL = 0;

    *GIC_PRIOMASK = ~0;
    for (int i = 0; i < 0x80; i++) {
        *GIC_IRQEND = i;
    }

    for (int i = 0; i < (0x20/4); i++) {
        DIC_CLRENABLE[i] = ~0;
        DIC_PRIORITY[i] = 0;
    }

    while(*GIC_PENDING != SPURIOUS_IRQ) {
        for (int i=0; i < (0x20/4); i++) {
            DIC_CLRPENDING[i] = ~0;
        }
    }

    IRQ_BASE[1] = (u32)&main_irq_handler;
    IRQ_BASE[0] = 0xE51FF004;

    *GIC_CONTROL = 1;
    *DIC_CONTROL = 1;

    return;
}
