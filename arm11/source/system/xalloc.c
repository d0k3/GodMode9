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

// super simple watermark allocator for ARM9 <-> ARM11 xfers
// designed to be request once, free never

#include "system/xalloc.h"

static char ALIGN(4096) xalloc_buf[XALLOC_BUF_SIZE];
static size_t mark = 0;

void *XAlloc(size_t size)
{ // not thread-safe at all
	void *ret;
	size_t end = size + mark;
	if (end >= XALLOC_BUF_SIZE)
		return NULL;

	ret = &xalloc_buf[mark];
	mark = end;
	return ret;
}
