#pragma once

/*
 *   This file is part of fastboot 3DS
 *   Copyright (C) 2017 derrek, profi200
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
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

#include "common.h"


#define NVRAM_SIZE  0x20000 // 1 Mbit (128kiB)  
#define SPIFLASH_CMD_RDSR  (0x05)
#define SPIFLASH_CMD_READ  (0x03)



// true if spiflash is installed, false otherwise
bool spiflash_get_status();
void spiflash_read(u32 offset, u32 size, u8 *buf);
