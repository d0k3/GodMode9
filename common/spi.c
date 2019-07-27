// Somewhat based on xerpi's SPI driver for Linux
/*
 *   This file is part of GodMode9
 *   Copyright (C) 2016 Sergi Granell
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

#include <common.h>
#include <types.h>

#include "spi.h"

#define REG_CFG_SPI_CNT	((vu16*)0x101401C0)

#define REG_SPI(bus, reg)	(*((vu32*)((bus) + (reg))))

#define REG_SPI_BUS0	(0x10160800)
#define REG_SPI_BUS1	(0x10142800)
#define REG_SPI_BUS2	(0x10143800)
#define REG_SPI_CARD	(0x1000D800)

#define REG_SPI_CONTROL	0x00
#define REG_SPI_DONE	0x04
#define REG_SPI_BLKLEN	0x08
#define REG_SPI_FIFO	0x0C
#define REG_SPI_STAT	0x10

#define SPI_CONTROL_RATE(n)	(n)
#define SPI_CONTROL_CS(n)	((n) << 6)
#define SPI_DIRECTION_READ	(0)
#define SPI_DIRECTION_WRITE	BIT(13)
#define SPI_CONTROL_BUSY	BIT(15)
#define SPI_CONTROL_START	BIT(15)

#define SPI_STAT_BUSY	BIT(0)

#define SPI_FIFO_WIDTH	(32)

static struct {
	u32 bus;
	u32 reg;
} SPI_Devices[] = {
	{REG_SPI_BUS0, SPI_CONTROL_RATE(2) | SPI_CONTROL_CS(0)},
	{REG_SPI_BUS0, SPI_CONTROL_RATE(0) | SPI_CONTROL_CS(1)}, // NVRAM
	{REG_SPI_BUS0, SPI_CONTROL_RATE(0) | SPI_CONTROL_CS(2)},
	{REG_SPI_BUS1, SPI_CONTROL_RATE(5) | SPI_CONTROL_CS(0)}, // CODEC
	{REG_SPI_CARD, SPI_CONTROL_RATE(3) | SPI_CONTROL_CS(0)}, // Gamecard flash chip
	{REG_SPI_CARD, SPI_CONTROL_RATE(1) | SPI_CONTROL_CS(0)}, // Gamecard IR chip
	// TODO: complete this table
};

static void SPI_WaitBusy(u32 bus)
{
	while(REG_SPI(bus, REG_SPI_CONTROL) & SPI_CONTROL_BUSY);
}

static void SPI_WaitFIFO(u32 bus)
{
	while(REG_SPI(bus, REG_SPI_STAT) & SPI_STAT_BUSY);
}

static void SPI_Done(u32 bus)
{
	REG_SPI(bus, REG_SPI_DONE) = 0;
}

static void SPI_SingleXfer(u32 reg, u32 bus, void *buffer, u32 len, bool read)
{
	u32 pos = 0;
	bool aligned = ((u32)buffer % 4 == 0) && (len % 4 == 0);

	REG_SPI(bus, REG_SPI_BLKLEN) = len;
	REG_SPI(bus, REG_SPI_CONTROL) = reg |
		(read ? SPI_DIRECTION_READ : SPI_DIRECTION_WRITE) | SPI_CONTROL_START;

	SPI_WaitFIFO(bus);

	do {
		if ((pos % SPI_FIFO_WIDTH) == 0)
			SPI_WaitFIFO(bus);

		if(aligned) {
			if (read) {
				((u32*)buffer)[pos / 4] = REG_SPI(bus, REG_SPI_FIFO);
			} else {
				REG_SPI(bus, REG_SPI_FIFO) = ((u32*)buffer)[pos / 4];
			}
		} else {
			if (read) {
				u32 tmp = REG_SPI(bus, REG_SPI_FIFO);
				memcpy((u8 *) buffer + pos, &tmp, min(4, len - pos));
			} else {
				u32 tmp;
				memcpy(&tmp, (u8 *) buffer + pos, min(4, len - pos));
				REG_SPI(bus, REG_SPI_FIFO) = tmp;
			}
		}

		pos += 4;
	} while(pos < len);
}

int SPI_DoXfer(u32 dev, const SPI_XferInfo *xfers, u32 xfer_cnt, bool done)
{
	u32 bus, reg;

	bus = SPI_Devices[dev].bus;
	reg = SPI_Devices[dev].reg;

	for (u32 i = 0; i < xfer_cnt; i++) {
		const SPI_XferInfo *xfer = &xfers[i];

		if (!xfer->buf || !xfer->len)
			continue;

		SPI_WaitBusy(bus);
		SPI_SingleXfer(reg, bus, xfer->buf, xfer->len, xfer->read);
	}

	SPI_WaitBusy(bus);
	if(done) {
		SPI_Done(bus);
	}
	return 0;
}

void SPI_Init(void)
{
	// This cuts off access to the old NDS SPI interface
	*REG_CFG_SPI_CNT = 7;
}

void SPI_Deinit(void)
{
	// Keep backwards compatibility with software that
	// assumes all bus interfaces will be set to old
	*REG_CFG_SPI_CNT = 0;
}
