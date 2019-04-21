// Somewhat based on xerpi's SPI driver for Linux
// Original comment follows:
/*
 *  nintendo3ds_spi.c
 *
 *  Copyright (C) 2016 Sergi Granell (xerpi)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <common.h>
#include <types.h>

#include "hw/spi.h"

#define REG_CFG_SPI_CNT	((vu16*)0x101401C0)

#define REG_SPI(bus, reg)	(*((vu32*)((bus) + (reg))))

#define REG_SPI_BUS0	(0x10160800)
#define REG_SPI_BUS1	(0x10142800)
#define REG_SPI_BUS2	(0x10143800)

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
	u32 regcfg;
} SPI_Devices[] = {
	{REG_SPI_BUS0, SPI_CONTROL_RATE(2) | SPI_CONTROL_CS(0)}, // device 0
	{REG_SPI_BUS0, SPI_CONTROL_RATE(0) | SPI_CONTROL_CS(1)},
	{REG_SPI_BUS0, SPI_CONTROL_RATE(0) | SPI_CONTROL_CS(2)},
	{REG_SPI_BUS1, SPI_CONTROL_RATE(5) | SPI_CONTROL_CS(0)},
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

static void SPI_SingleXfer(u32 reg, u32 bus, u32 *buffer, u32 len, bool read)
{
	u32 pos = 0;

	REG_SPI(bus, REG_SPI_BLKLEN) = len;
	REG_SPI(bus, REG_SPI_CONTROL) = reg |
		(read ? SPI_DIRECTION_READ : SPI_DIRECTION_WRITE) | SPI_CONTROL_START;

	do {
		if ((pos % SPI_FIFO_WIDTH) == 0)
			SPI_WaitFIFO(bus);

		if (read) {
			buffer[pos / 4] = REG_SPI(bus, REG_SPI_FIFO);
		} else {
			REG_SPI(bus, REG_SPI_FIFO) = buffer[pos / 4];
		}

		pos += 4;
	} while(pos < len);
}

int SPI_DoXfer(u32 dev, SPI_XferInfo *xfers, u32 xfer_cnt)
{
	u32 bus;
	u32 dev_reg;

	bus = SPI_Devices[dev].bus;
	dev_reg = SPI_Devices[dev].regcfg;

	for (u32 i = 0; i < xfer_cnt; i++) {
		SPI_XferInfo *xfer = &xfers[i];

		SPI_WaitBusy(bus);
		SPI_SingleXfer(dev_reg, bus, xfer->buf, xfer->len, xfer->read);
	}

	SPI_WaitBusy(bus);
	SPI_Done(bus);
	return 0;
}

void SPI_Init(void)
{
	// This cuts off access from the old SPI
	// interface used during the NDS days
	*REG_CFG_SPI_CNT = 7;
}
