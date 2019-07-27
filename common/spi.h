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

#define SPI_DEV_NVRAM 1
#define SPI_DEV_CODEC 3
#define SPI_DEV_CART_FLASH 4
#define SPI_DEV_CART_IR 5

typedef struct {
	void *buf;
	u32 len;
	bool read;
} SPI_XferInfo;

int SPI_DoXfer(u32 dev, const SPI_XferInfo *xfer, u32 xfer_cnt, bool done);

void SPI_Init(void);
void SPI_Deinit(void);
