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

typedef struct {
	u64 hid_state;
} GlobalSharedMemory;

#ifdef ARM9
#include <pxi.h>

static inline const GlobalSharedMemory *ARM_GetSHMEM(void)
{
	return (const GlobalSharedMemory*)ARM_GetTID();
}

static void ARM_InitSHMEM(void)
{
	ARM_SetTID(PXI_DoCMD(PXI_GET_SHMEM, NULL, 0));
}
#endif
