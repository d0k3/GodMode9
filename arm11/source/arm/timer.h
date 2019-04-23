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

#include <types.h>

// The timer interval is calculated using the following equation:
// T = [(PRESCALER_value + 1) * (Load_value + 1) * 2] / CPU_CLK
// therefore
// Load_value = [(CPU_CLK / 2) * (T / (PRESCALER_value + 1))] - 1

#define BASE_CLKRATE	(268111856 / 2)

#define CLK_MS_TO_TICKS(m)	(((BASE_CLKRATE / 1000) * (m)) - 1)

void TIMER_WaitTicks(u32 ticks);
