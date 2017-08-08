/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#include <types.h>
#include <cpu.h>
#include <gic.h>

#define IRQ_BASE ((vu32*)0x1FFFFFA0)

irq_handler handler_table[MAX_IRQ];

void __attribute__((interrupt("IRQ"))) gic_irq_handler(void)
{
    u32 xrq, ss;
    CPU_EnterCritical(&ss);
    xrq = *GIC_IRQACK;
    if (xrq < MAX_IRQ && handler_table[xrq]) {
        (handler_table[xrq])(xrq);
    }
    *GIC_IRQEND = xrq;
    CPU_LeaveCritical(&ss);
    return;
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
    *GIC_CONTROL = 0;
    *GIC_PRIOMASK = ~0;

    for (int i = 0; i < (BIT(9)-1); i++) {
        *GIC_IRQEND |= i;
    }

    *DIC_CONTROL = 0;
    for (int i = 0; i < (0x20/4); i++) {
        DIC_CLRENABLE[i] = ~0;
        DIC_PRIORITY[i] = 0;
    }

    *DIC_CONTROL = 1;
    *GIC_CONTROL = 1;

    IRQ_BASE[1] = (u32)gic_irq_handler;
    IRQ_BASE[0] = 0xE51FF004;
    return;
}
