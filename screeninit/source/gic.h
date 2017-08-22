#pragma once
#include <types.h>

typedef void (*irq_handler)(void);

#define MAX_IRQ      (0x80)

#define SPURIOUS_IRQ (1023)

#define GIC_BASE (0x17E00100)
#define DIC_BASE (0x17E01000)

#define GIC_CONTROL    ((vu32*)(GIC_BASE + 0x00))
#define GIC_PRIOMASK   ((vu32*)(GIC_BASE + 0x04))
#define GIC_IRQACK     ((vu32*)(GIC_BASE + 0x0C))
#define GIC_IRQEND     ((vu32*)(GIC_BASE + 0x10))
#define GIC_PENDING    ((vu32*)(GIC_BASE + 0x18))

#define DIC_CONTROL    ((vu32*)(DIC_BASE + 0x000))
#define DIC_SETENABLE  ((vu32*)(DIC_BASE + 0x100))
#define DIC_CLRENABLE  ((vu32*)(DIC_BASE + 0x180))
#define DIC_SETPENDING ((vu32*)(DIC_BASE + 0x200))
#define DIC_CLRPENDING ((vu32*)(DIC_BASE + 0x280))
#define DIC_PRIORITY   ((vu32*)(DIC_BASE + 0x400))
#define DIC_PROCTGT    ((vu8*) (DIC_BASE + 0x800))
#define DIC_CFGREG     ((vu32*)(DIC_BASE + 0xC00))

void GIC_SetIRQ(u32 irq_id, irq_handler hndl);
void GIC_Reset(void);
