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

#define DEF_SECT_(n)	extern u32 __##n##_pa, __##n##_va, __##n##_len;
DEF_SECT_(vector)
DEF_SECT_(text)
DEF_SECT_(data)
DEF_SECT_(rodata)
DEF_SECT_(bss)
#undef DEF_SECT_

#define SECTION_VA(n)	((u32)&__##n##_va)
#define SECTION_PA(n)	((u32)&__##n##_pa)
#define SECTION_LEN(n)	((u32)&__##n##_len)

#define SECTION_TRI(n)	SECTION_VA(n), SECTION_PA(n), SECTION_LEN(n)
