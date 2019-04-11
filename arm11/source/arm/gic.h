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
	GIC_LOWEST_PRIO = 0xF,
};

void GIC_GlobalReset(void);
void GIC_LocalReset(void);

int GIC_Enable(u32 irqn, u32 coremask, u32 prio, IRQ_Handler handler);
int GIC_Disable(u32 irqn, u32 coremask);

void GIC_TriggerSoftIRQ(u32 irqn, u32 mode, u32 coremask);
