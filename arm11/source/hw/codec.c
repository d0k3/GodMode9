// Based on xerpi's CODEC driver for Linux
// Original comment follows:
/*
 * nintendo3ds_codec.c
 *
 * Copyright (C) 2016 Sergi Granell (xerpi)
 * Copyright (C) 2017 Paul LaMendola (paulguy)
 * based on ad7879-spi.c
 *
 * Licensed under the GPL-2 or later.
 */

#include <common.h>
#include <types.h>

#include <hid_map.h>

#include "hw/codec.h"
#include "hw/spi.h"

#define CODEC_SPI_DEV	(3)

#define MAX_12BIT	(BIT(12) - 1)
#define CPAD_THRESH	(150)
#define CPAD_FACTOR (150)

/* SPI stuff */
static void CODEC_DualTX(u8 *tx0, u8 len0, u8 *tx1, u8 len1)
{
	SPI_XferInfo xfers[2];

	xfers[0].buf = (u32*)tx0;
	xfers[0].len = len0;
	xfers[0].read = false;

	xfers[1].buf = (u32*)tx1;
	xfers[1].len = len1;
	xfers[1].read = false;

	SPI_DoXfer(CODEC_SPI_DEV, xfers, 2);
}

static void CODEC_WriteRead(u8 *tx_buf, u8 tx_len,
			  u8 *rx_buf, u8 rx_len)
{
	SPI_XferInfo xfers[2];

	xfers[0].buf = (u32*)tx_buf;
	xfers[0].len = tx_len;
	xfers[0].read = false;

	xfers[1].buf = (u32*)rx_buf;
	xfers[1].len = rx_len;
	xfers[1].read = true;

	SPI_DoXfer(CODEC_SPI_DEV, xfers, 2);
}

static void CODEC_RegSelect(u8 reg)
{
	u8 buffer1[4];
	u8 buffer2[0x40];

	buffer1[0] = 0;
	buffer2[0] = reg;

	CODEC_DualTX(buffer1, 1, buffer2, 1);
}

static u8 CODEC_RegRead(u8 offset)
{
	u8 buffer_wr[8];
	u8 buffer_rd[0x40];

	buffer_wr[0] = 1 | (offset << 1);

	CODEC_WriteRead(buffer_wr, 1, buffer_rd, 1);

	return buffer_rd[0];
}

static void CODEC_RegWrite(u8 reg, u8 val)
{
	u8 buffer1[8];
	u8 buffer2[0x40];

	buffer1[0] = (reg << 1); // Write
	buffer2[0] = val;

	CODEC_DualTX(buffer1, 1, buffer2, 1);
}

static void CODEC_RegReadBuf(u8 offset, void *buffer, u8 size)
{
	u8 buffer_wr[0x10];

	buffer_wr[0] = 1 | (offset << 1);

	CODEC_WriteRead(buffer_wr, 1, buffer, size);
}

static void CODEC_RegMask(u8 offset, u8 mask0, u8 mask1)
{
	u8 buffer1[4];
	u8 buffer2[0x40];

	buffer1[0] = 1 | (offset << 1);

	CODEC_WriteRead(buffer1, 1, buffer2, 1);

	buffer1[0] = offset << 1;
	buffer2[0] = (buffer2[0] & ~mask1) | (mask0 & mask1);

	CODEC_DualTX(buffer1, 1, buffer2, 1);
}

void CODEC_Init(void)
{
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x24, 0x98);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x26, 0x00);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x25, 0x43);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x24, 0x18);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x17, 0x43);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x19, 0x69);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x1B, 0x80);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x27, 0x11);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x26, 0xEC);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x24, 0x18);
	CODEC_RegSelect(0x67);
	CODEC_RegWrite(0x25, 0x53);

	CODEC_RegSelect(0x67);
	CODEC_RegMask(0x26, 0x80, 0x80);
	CODEC_RegSelect(0x67);
	CODEC_RegMask(0x24, 0x00, 0x80);
	CODEC_RegSelect(0x67);
	CODEC_RegMask(0x25, 0x10, 0x3C);
}

static void CODEC_GetRawData(u8 *buffer)
{
	CODEC_RegSelect(0x67);
	CODEC_RegRead(0x26);
	CODEC_RegSelect(0xFB);
	CODEC_RegReadBuf(1, buffer, 0x34);
}

void CODEC_Get(CODEC_Input *input)
{
	u8 raw_data[0x34] = {0};
	s16 cpad_x, cpad_y;
	bool ts_pressed;

	CODEC_GetRawData(raw_data);

	cpad_x = ((raw_data[0x24] << 8 | raw_data[0x25]) & 0xFFF) - 2048;
	cpad_y = ((raw_data[0x14] << 8 | raw_data[0x15]) & 0xFFF) - 2048;

	// X axis is inverted
	if (abs(cpad_x) > CPAD_THRESH)
		input->cpad_x = -cpad_x / CPAD_FACTOR;
	else
		input->cpad_x = 0;

	if (abs(cpad_y) > CPAD_THRESH)
		input->cpad_y = cpad_y / CPAD_FACTOR;
	else
		input->cpad_y = 0;

	ts_pressed = !(raw_data[0] & BIT(4));
	if (ts_pressed) {
		input->ts_x = (raw_data[0] << 8) | raw_data[1];
		input->ts_y = (raw_data[10] << 8) | raw_data[11];
	} else {
		input->ts_x = 0xFFFF;
		input->ts_y = 0xFFFF;
	}
}
