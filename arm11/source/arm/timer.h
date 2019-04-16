#pragma once

#include <types.h>

// The timer interval is calculated using the following equation:
// T = [(PRESCALER_value + 1) * (Load_value + 1) * 2] / CPU_CLK
// therefore
// Load_value = [(CPU_CLK / 2) * (T / (PRESCALER_value + 1))] - 1

#define BASE_CLKRATE	(268111856)

#define CLK_FREQ_TO_INTERVAL(f, p)	((BASE_CLKRATE/2) * ((f) / ((p) + 1)) - 1)
