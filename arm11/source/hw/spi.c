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

#define CFG_SPI_CNT	((vu32*)0x101401C0)

// TODO: CURRENTLY HARDCODED FOR DEVICE 3 (TOUCHSCREEN)
// IF ANY OTHER DEVICES ARE TO BE USED, ANOTHER BUS MUST
// BE ACCESSED, CHECK 3dbrew.org/wiki/SPI_{Registers, Services}
static const u32 SPI_Buses[] = { 0x10142800 };
#define REG_SPI(b, n)	(*(vu32*)(SPI_Buses[b] + (n)))

#define REG_SPI_CNT		REG_SPI(0, 0x00)
#define REG_SPI_DONE 	REG_SPI(0, 0x04)
#define REG_SPI_BLKLEN	REG_SPI(0, 0x08)
#define REG_SPI_FIFO	REG_SPI(0, 0x0C)
#define REG_SPI_STATUS	REG_SPI(0, 0x10)

#define SPI_CNT_BUSY	BIT(15)
#define SPI_CNT_START	BIT(15)

#define SPI_CNT_READ	(0)
#define SPI_CNT_WRITE	BIT(13)

#define SPI_CNT_RATE(n)	(n)
#define SPI_CNT_CS(n)	((n) << 6)

#define SPI_STAT_BUSY	BIT(0)

#define SPI_FIFO_WIDTH	(32)

static u8 SPI_GetDevSelect(u32 dev)
{
	static const u8 SPI_DevSelect[] = { 0, 1, 2, 0, 1, 2 };
	if (dev < countof(SPI_DevSelect)) {
		return SPI_DevSelect[dev];
	} else {
		return 0;
	}
}

static u8 SPI_GetDevBaudrate(u32 dev)
{
	static const u8 SPI_BaudRates[] = { 2, 0, 0, 5 };
	if (dev < countof(SPI_BaudRates)) {
		return SPI_BaudRates[dev];
	} else {
		return 0;
	}
}

static void SPI_WaitBusy(void)
{
	while(REG_SPI_CNT & SPI_CNT_BUSY);
}

static void SPI_WaitFIFO(void)
{
	while(REG_SPI_STATUS & SPI_STAT_BUSY);
}

static void SPI_Done(void)
{
	REG_SPI_DONE = 0;
}

static void SPI_SingleXfer(u32 reg, bool read, u32 *buffer, u32 len)
{
	u32 pos = 0;

	REG_SPI_BLKLEN = len;
	REG_SPI_CNT = reg | (read ? SPI_CNT_READ : SPI_CNT_WRITE) | SPI_CNT_START;

	do {
		if ((pos % SPI_FIFO_WIDTH) == 0)
			SPI_WaitFIFO();

		if (read) {
			buffer[pos / 4] = REG_SPI_FIFO;
		} else {
			REG_SPI_FIFO = buffer[pos / 4];
		}

		pos += 4;
	} while(pos < len);
}

int SPI_DoXfer(u32 dev, SPI_XferInfo *xfers, u32 xfer_cnt)
{
	u32 dev_cfg;
	int baud, cs;

	baud = SPI_GetDevBaudrate(dev);
	cs = SPI_GetDevSelect(dev);
	dev_cfg = SPI_CNT_RATE(baud) | SPI_CNT_CS(cs);

	for (u32 i = 0; i < xfer_cnt; i++) {
		SPI_XferInfo *xfer = &xfers[i];

		SPI_WaitBusy();
		SPI_SingleXfer(dev_cfg, xfer->read, xfer->buf, xfer->len);
	}

	SPI_WaitBusy();
	SPI_Done();
	return 0;
}

void SPI_Init(void)
{
	// Hack: here all registers should be set to the "new" mode
	// but GM9 uses the old interface to access NVRAM
	// as such, only the bus used by CODEC will be set to new
	// *CFG_SPI_CNT = 7;
	*CFG_SPI_CNT = BIT(1);
}
