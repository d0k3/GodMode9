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

enum MMU_MemoryType {
	STRONGLY_ORDERED = 0,
	NON_CACHEABLE,
	DEVICE_SHARED,
	DEVICE_NONSHARED,
	CACHED_WT,
	CACHED_WB,
	CACHED_WB_ALLOC,
	MEMORY_TYPES,
};

enum MMU_MemoryAccess {
	NO_ACCESS = 0,
	READ_ONLY,
	READ_WRITE,
};

#define MMU_FLAGS(t, ap, nx, s)	((s) << 25 | (nx) << 24 | (ap) << 8 | (t))

#define MMU_FLAGS_TYPE(f)	((f) & 0xFF)
#define MMU_FLAGS_ACCESS(f)	(((f) >> 8) & 0xFF)

#define MMU_FLAGS_NOEXEC(f)	((f) & BIT(24))
#define MMU_FLAGS_SHARED(f)	((f) & BIT(25))

u32 MMU_Map(u32 va, u32 pa, u32 size, u32 flags);
void MMU_Init(void);
