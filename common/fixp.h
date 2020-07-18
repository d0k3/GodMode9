/*
 *   This file is part of GodMode9
 *   Copyright (C) 2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#define FIXP_UNIT_MASK	(~FIXP_FRAC_MASK)

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

static inline fixp_t fixp_changespace(fixp_t n, fixp_t lower_s, fixp_t upper_s, fixp_t lower_d, fixp_t upper_d)
{
	return fixp_product(n - lower_s, fixp_quotient(upper_d, upper_s)) + lower_d;
}
