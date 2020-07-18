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

#include <types.h>

#include <spi.h>
#include "hw/nvram.h"

// returns manuf id, memory type and size
// size = (1 << id[2]) ?
// apparently unreliable on some Sanyo chips?
#define CMD_RDID	0x9F

#define CMD_READ	0x03
#define CMD_WREN	0x06
#define CMD_WRDI	0x04

#define CMD_RDSR	0x05

#define CMD_DPD	0xB9 // deep power down
#define CMD_RDP	0xAB // release from deep power down

static u32 NVRAM_SendStatusCommand(u32 cmd, u32 width)
{
	u32 ret;
	SPI_XferInfo xfer[2];

	xfer[0].buf = &cmd;
	xfer[0].len = 1;
	xfer[0].read = false;

	xfer[1].buf = &ret;
	xfer[1].len = width;
	xfer[1].read = true;

	ret = 0;
	SPI_DoXfer(SPI_DEV_NVRAM, xfer, 2, true);
	return ret;
}

u32 NVRAM_Status(void)
{
	return NVRAM_SendStatusCommand(CMD_RDSR, 1);
}

u32 NVRAM_ReadID(void)
{
	return NVRAM_SendStatusCommand(CMD_RDID, 3);
}

void NVRAM_DeepStandby(void)
{
	NVRAM_SendStatusCommand(CMD_DPD, 0);
}

void NVRAM_Wakeup(void)
{
	NVRAM_SendStatusCommand(CMD_RDP, 0);
}

void NVRAM_Read(u32 address, u32 *buffer, u32 len)
{
	SPI_XferInfo xfer[2];
	u32 cmd;

	address &= BIT(24) - 1;
	cmd = __builtin_bswap32(address) | CMD_READ;

	xfer[0].buf = &cmd;
	xfer[0].len = 4;
	xfer[0].read = false;

	xfer[1].buf = buffer;
	xfer[1].len = len;
	xfer[1].read = true;

	SPI_DoXfer(SPI_DEV_NVRAM, xfer, 2, true);
}
