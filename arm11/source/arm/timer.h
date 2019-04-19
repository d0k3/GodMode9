#pragma once

#include <types.h>

// The timer interval is calculated using the following equation:
// T = [(PRESCALER_value + 1) * (Load_value + 1) * 2] / CPU_CLK
// therefore
// Load_value = [(CPU_CLK / 2) * (T / (PRESCALER_value + 1))] - 1

#define BASE_CLKRATE	(268111856 / 2)

#define CLK_MS_TO_TICKS(m)	(((BASE_CLKRATE / 1000) * (m)) - 1)

void TIMER_WaitTicks(u32 ticks);
