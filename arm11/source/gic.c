#include <types.h>
#include <cpu.h>
#include <gic.h>

#include <string.h>

#define IRQVECTOR_BASE ((vu32*)0x1FFFFFA0)

extern void (*main_irq_handler)(void);
irq_handler GIC_Handlers[128];

void GIC_Reset(void)
{
    u32 irq_s;

    REG_GIC_CONTROL = 0;
    memset(GIC_Handlers, 0, sizeof(GIC_Handlers));

    REG_DIC_CONTROL = 0;
    for (int i = 0; i < 4; i++) {
        REG_DIC_CLRENABLE[i]  = ~0;
        REG_DIC_CLRPENDING[i] = ~0;
    }
    for (int i = 0; i < 32; i++)   REG_DIC_PRIORITY[i] = 0;
    for (int i = 32; i < 128; i++) REG_DIC_TARGETPROC[i] = 0;
    for (int i = 0; i < 8; i++)    REG_DIC_CFGREG[i] = ~0;
    REG_DIC_CONTROL = 1;

    REG_DIC_CLRENABLE[0] = ~0;
    for (int i = 0; i < 32; i++) REG_DIC_PRIORITY[i] = 0;
    for (int i = 0; i < 2; i++) REG_DIC_CFGREG[i] = ~0;
    REG_GIC_POI = 3;
    REG_GIC_PRIOMASK = 0xF << 4;
    REG_GIC_CONTROL = 1;

    do {
        irq_s = REG_GIC_PENDING;
        REG_GIC_IRQEND = irq_s;
    } while(irq_s != 1023);

    IRQVECTOR_BASE[1] = (u32)&main_irq_handler;
    IRQVECTOR_BASE[0] = 0xE51FF004;
}

void GIC_SetIRQ(u32 irq, irq_handler handler)
{
    GIC_Handlers[irq] = handler;
    REG_DIC_CLRPENDING[irq >> 5] |= BIT(irq & 0x1F);
    REG_DIC_SETENABLE[irq >> 5]  |= BIT(irq & 0x1F);
    REG_DIC_TARGETPROC[irq] = 1;
}
