#pragma once

// Fixed point math operations

#include <types.h>
#include <limits.h>

typedef int32_t fixp_t;

// 12 bit precision was chosen because
// that's the touchscreen's ADC resolution
#define FIXP_PRECISION	(12)

#define INT_TO_FIXP(i)	((fixp_t)((i) * (1 << FIXP_PRECISION)))
#define FIXP_TO_INT(f)	((fixp_t)((f) / (1 << FIXP_PRECISION)))

#define FIXP_WHOLE_UNIT	INT_TO_FIXP(1)
#define FIXP_HALF_UNIT	(FIXP_WHOLE_UNIT / 2)
#define FIXP_ZERO_UNIT	(0)

#define FIXP_FRAC_MASK	(FIXP_WHOLE_UNIT - 1)
#define FIXP_UNIT_MASK	(~0 & ~FIXP_FRAC_MASK)

static inline fixp_t fixp_product(fixp_t a, fixp_t b)
{
	return (((s64)a * (s64)b) >> FIXP_PRECISION);
}

static inline fixp_t fixp_quotient(fixp_t a, fixp_t b)
{
	return ((s64)a << FIXP_PRECISION) / b;
}

static inline fixp_t fixp_round(fixp_t n)
{
	return (n + FIXP_HALF_UNIT) & FIXP_UNIT_MASK;
}
