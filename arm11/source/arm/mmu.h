/*
 *   This file is part of GodMode9
 *   Copyright (C) 2018-2019 Wolfvak
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

enum {
	MMU_STRONG_ORDER = 0,
	MMU_UNCACHEABLE,
	MMU_DEV_SHARED,
	MMU_DEV_NONSHARED,
	MMU_CACHE_WT,
	MMU_CACHE_WB,
	MMU_CACHE_WBA,
	MMU_MEMORY_TYPES,
};

enum {
	MMU_NO_ACCESS = 0,
	MMU_READ_ONLY,
	MMU_READ_WRITE,
	MMU_ACCESS_TYPES,
};

#define MMU_FLAGS(t, ap, nx, s)	((s) << 25 | (nx) << 24 | (ap) << 8 | (t))

#define MMU_FLAGS_TYPE(f)	((f) & 0xFF)
#define MMU_FLAGS_ACCESS(f)	(((f) >> 8) & 0xFF)

#define MMU_FLAGS_NOEXEC(f)	((f) & BIT(24))
#define MMU_FLAGS_SHARED(f)	((f) & BIT(25))

u32 mmuMapArea(u32 va, u32 pa, u32 size, u32 flags);

void mmuInvalidate(void);
void mmuInvalidateVA(u32 addr); // DO NOT USE

void mmuInitRegisters(void);
