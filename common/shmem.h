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

#include <arm.h>

#define SHMEM_BUFFER_SIZE 2048

typedef struct {
	union {
		struct { u32 keys, touch; };
		u64 full;
	} hidState;

	union {
		uint8_t b[SHMEM_BUFFER_SIZE];
		uint16_t s[SHMEM_BUFFER_SIZE / 2];
		uint32_t w[SHMEM_BUFFER_SIZE / 4];
		uint64_t q[SHMEM_BUFFER_SIZE / 8];
	} dataBuffer;
} __attribute__((packed, aligned(8))) SystemSHMEM;

#ifdef ARM9
#include <pxi.h>

extern SystemSHMEM *shmemBasePtr;

static inline SystemSHMEM *ARM_GetSHMEM(void)
{
	// shared memory contents are extremely likely to change
	// insert a compiler barrier to force the compiler not to assume
	// memory values will remain constant in between calls to getSHMEM
	asm_v("":::"memory", "cc");
	return shmemBasePtr;
}

static inline void ARM_InitSHMEM(void)
{
	shmemBasePtr = (SystemSHMEM*)PXI_DoCMD(PXICMD_GET_SHMEM_ADDRESS, NULL, 0);
}
#endif
