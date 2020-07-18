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

#include <spi.h>

#define NVRAM_SR_WIP	BIT(0) // work in progress / busy
#define NVRAM_SR_WEL	BIT(1) // write enable latch

u32 NVRAM_Status(void);
u32 NVRAM_ReadID(void);

void NVRAM_Read(u32 offset, u32 *buffer, u32 len);

void NVRAM_DeepStandby(void);
void NVRAM_Wakeup(void);
