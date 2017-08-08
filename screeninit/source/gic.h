/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

#pragma once
#include <types.h>

typedef void (*irq_handler)(u32);

#define MAX_IRQ  (0x80)

#define GIC_BASE (0x17E00100)
#define DIC_BASE (0x17E01000)

/* Setting bit 0 enables the GIC */
#define GIC_CONTROL   ((vu32*)(GIC_BASE + 0x00))
/* Bits [7:0] control the min priority accepted */
#define GIC_PRIOMASK  ((vu32*)(GIC_BASE + 0x04))
/* When an IRQ occurrs, this register holds the IRQ ID */
#define GIC_IRQACK    ((vu32*)(GIC_BASE + 0x0C))
/* Write the IRQ ID here to acknowledge it */
#define GIC_IRQEND    ((vu32*)(GIC_BASE + 0x10))


/* Setting bit 0 enables the DIC */
#define DIC_CONTROL   ((vu32*)(DIC_BASE + 0x000))
/*
 Write here to enable an IRQ ID
 The register address is DIC_SETENABLE + (N/32)*4 and its
 corresponding bit index is (N%32)
*/
#define DIC_SETENABLE ((vu32*)(DIC_BASE + 0x100))

/* same as above but disables the IRQ */
#define DIC_CLRENABLE ((vu32*)(DIC_BASE + 0x180))

/* sets the IRQ priority */
#define DIC_PRIORITY  ((vu32*)(DIC_BASE + 0x400))

/* specifies which CPUs are allowed to be forwarded the IRQ */
#define DIC_PROCTGT   ((vu8*)(DIC_BASE + 0x800))

/*
 each irq has 2 bits assigned
 bit 0 = 0: uses 1-N model
         1: uses N-N model

 bit 1 = 0: level high active
         1: rising edge sensitive
 */
#define DIC_CFGREG    ((vu32*)(DIC_BASE + 0xC00))

void gic_irq_handler(void);
void GIC_Configure(u32 irq_id, irq_handler hndl);
void GIC_Reset(void);
