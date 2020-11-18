#pragma once

#include <types.h>

typedef struct {
	void (*reset)(void);
	u32 (*test)(u32 param, u32 clear);
} EventInterface;

const EventInterface *getEventIRQ(void);
const EventInterface *getEventMCU(void);

static inline void eventReset(const EventInterface *ei) {
	ei->reset();
}

static inline u32 eventTest(const EventInterface *ei, u32 param, u32 clear) {
	return ei->test(param, clear);
}

static inline u32 eventWait(const EventInterface *ei, u32 param, u32 clear) {
	while(1) {
		u32 ret = ei->test(param, clear);
		if (ret) return ret;
	}
}
