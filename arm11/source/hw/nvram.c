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
#include "arm/timer.h"
#include "hw/nvram.h"

// returns manuf id, memory type and size
// size = (1 << id[2]) ?
// apparently unreliable on some Sanyo chips?
#define CMD_RDID	0x9F

#define CMD_READ	0x03
#define CMD_WREN	0x06
#define CMD_WRDI	0x04
#define CMD_WRITE	0x0A

#define CMD_RDSR	0x05

#define NVRAM_SR_WIP	BIT(0) // work in progress / busy
#define NVRAM_SR_WEL	BIT(1) // write enable latch

#define CMD_DPD	0xB9 // deep power down
#define CMD_RDP	0xAB // release from deep power down

#define NVRAM_PAGE_SIZE	0x100		// 256 byte pages
#define NVRAM_ADDR_MASK	0xFFFFFF	// 24bit address
#define NVRAM_ADDR_MAX	(NVRAM_ADDR_MASK + 1)

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

static void NVRAM_SetWriteEnable(bool write_en)
{
	u8 cmd = write_en ? CMD_WREN : CMD_WRDI;
	SPI_XferInfo xfer;
	xfer.buf = &cmd;
	xfer.len = 1;
	xfer.read = false;
	SPI_DoXfer(SPI_DEV_NVRAM, &xfer, 1, true);
}

u32 NVRAM_Status(void)
{
	return NVRAM_SendStatusCommand(CMD_RDSR, 1);
}

void NVRAM_DeepStandby(void)
{
	NVRAM_SendStatusCommand(CMD_DPD, 0);
}

void NVRAM_Wakeup(void)
{
	NVRAM_SendStatusCommand(CMD_RDP, 0);
}

u32 NVRAM_ReadID(void)
{
	return NVRAM_SendStatusCommand(CMD_RDID, 3);
}

int NVRAM_Read(u32 address, u32 *buffer, u32 len)
{
	SPI_XferInfo xfer[2];
	u32 cmd;

	if (address >= NVRAM_ADDR_MAX || len > NVRAM_PAGE_SIZE || (address + len) > NVRAM_ADDR_MAX)
		return -1;

	cmd = __builtin_bswap32(address) | CMD_READ;

	xfer[0].buf = &cmd;
	xfer[0].len = 4;
	xfer[0].read = false;

	xfer[1].buf = buffer;
	xfer[1].len = len;
	xfer[1].read = true;

	SPI_DoXfer(SPI_DEV_NVRAM, xfer, 2, true);
	return 0;
}

static int NVRAM_WritePage(u32 address, const u32 *buffer, u32 len)
{
	SPI_XferInfo xfer[2];
	u32 cmd, i;

	if (address >= NVRAM_ADDR_MAX || len > NVRAM_PAGE_SIZE || (address + len) > NVRAM_ADDR_MAX)
		return -1;

	cmd = __builtin_bswap32(address) | CMD_WRITE;

	xfer[0].buf = &cmd;
	xfer[0].len = 4;
	xfer[0].read = false;

	xfer[1].buf = (void*)buffer;
	xfer[1].len = len;
	xfer[1].read = true;

	// enable the write latch
	NVRAM_SetWriteEnable(true);
	// make sure it's enabled
	for (i = 0; i <= 1000; i++) {
		if (i == 1000) return -2;
		if (NVRAM_Status() & NVRAM_SR_WEL) break;
		TIMER_WaitMS(1);
	}

	// do the write transfer
	SPI_DoXfer(SPI_DEV_NVRAM, xfer, 2, true);

	// wait until it's done, disable the write latch
	for (i = 0; i <= 1000; i++) {
		if (i == 1000) return -3;
		if (!(NVRAM_Status() & NVRAM_SR_WIP)) break;
		TIMER_WaitMS(1);
	}
	NVRAM_SetWriteEnable(false);

	return 0;
}

int NVRAM_Write(u32 address, const u32 *buffer, u32 len)
{
	while(len > 0) {
		u32 blksz = len < NVRAM_PAGE_SIZE ? len : NVRAM_PAGE_SIZE;

		int result = NVRAM_WritePage(address, buffer, blksz);
		if (result != 0)
			return result;

		address += blksz;
		buffer += (blksz / 4);
		len -= blksz;
	}

	return 0;
}
