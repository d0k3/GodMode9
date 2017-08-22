#include <types.h>
#include <cpu.h>
#include <gic.h>

#define IRQVECTOR_BASE ((vu32*)0x1FFFFFA0)

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

void GIC_SetIRQ(u32 irq_id, irq_handler hndl)
{
    handler_table[irq_id] = hndl;
    DIC_CLRENABLE[irq_id/32] |= BIT(irq_id & 0x1F);
    DIC_SETENABLE[irq_id/32] |= BIT(irq_id & 0x1F);
    DIC_PROCTGT[irq_id] = 1;
    return;
}

void GIC_Reset(void)
{
    *GIC_CONTROL = 1;
    *DIC_CONTROL = 1;

    *GIC_PRIOMASK = ~0;
    for (int i = 0; i < MAX_IRQ; i++) {
        handler_table[i] = NULL;
        *GIC_IRQEND = i;
    }

    for (int i = 0; i < (0x08); i++) {
        DIC_CLRENABLE[i] = ~0;
        DIC_PRIORITY[i] = 0;
    }

    while(*GIC_PENDING != SPURIOUS_IRQ) {
        for (int i=0; i < (0x08); i++) {
            DIC_CLRPENDING[i] = ~0;
        }
    }

    IRQVECTOR_BASE[1] = (u32)&main_irq_handler;
    IRQVECTOR_BASE[0] = 0xE51FF004;
    return;
}
