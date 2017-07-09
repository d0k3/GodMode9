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

#include <stdbool.h>
#include "i2c.h"


#define I2C1_REGS_BASE  (0x10161000)
#define REG_I2C1_DATA   *((vu8* )(I2C1_REGS_BASE + 0x00))
#define REG_I2C1_CNT    *((vu8* )(I2C1_REGS_BASE + 0x01))
#define REG_I2C1_CNTEX  *((vu16*)(I2C1_REGS_BASE + 0x02))
#define REG_I2C1_SCL    *((vu16*)(I2C1_REGS_BASE + 0x04))

#define I2C2_REGS_BASE  (0x10144000)
#define REG_I2C2_DATA   *((vu8* )(I2C2_REGS_BASE + 0x00))
#define REG_I2C2_CNT    *((vu8* )(I2C2_REGS_BASE + 0x01))
#define REG_I2C2_CNTEX  *((vu16*)(I2C2_REGS_BASE + 0x02))
#define REG_I2C2_SCL    *((vu16*)(I2C2_REGS_BASE + 0x04))

#define I2C3_REGS_BASE  (0x10148000)
#define REG_I2C3_DATA   *((vu8* )(I2C3_REGS_BASE + 0x00))
#define REG_I2C3_CNT    *((vu8* )(I2C3_REGS_BASE + 0x01))
#define REG_I2C3_CNTEX  *((vu16*)(I2C3_REGS_BASE + 0x02))
#define REG_I2C3_SCL    *((vu16*)(I2C3_REGS_BASE + 0x04))


static const struct
{
	u8 busId;
	u8 devAddr;
} i2cDevTable[] =
{
	{0,	0x4A},
	{0,	0x7A},
	{0,	0x78},
	{1,	0x4A},
	{1,	0x78},
	{1,	0x2C},
	{1,	0x2E},
	{1,	0x40},
	{1,	0x44},
	{2,	0xA6}, // TODO: Find out if 0xA6 or 0xD6 is correct
	{2,	0xD0},
	{2,	0xD2},
	{2,	0xA4},
	{2,	0x9A},
	{2,	0xA0},
	{1,	0xEE},
	{0,	0x40},
	{2,	0x54}
};



static void i2cWaitBusy(vu8 *cntReg)
{
	while(*cntReg & I2C_ENABLE);
}

static vu8* i2cGetBusRegsBase(u8 busId)
{
	vu8 *base;
	if(!busId)          base = (vu8*)I2C1_REGS_BASE;
	else if(busId == 1) base = (vu8*)I2C2_REGS_BASE;
	else                base = (vu8*)I2C3_REGS_BASE;

	return base;
}

static bool i2cStartTransfer(I2cDevice devId, u8 regAddr, bool read, vu8 *regsBase)
{
	const u8 devAddr = i2cDevTable[devId].devAddr;
	vu8 *const i2cData = regsBase;
	vu8 *const i2cCnt  = regsBase + 1;


	u32 i = 0;
	for(; i < 8; i++)
	{
		i2cWaitBusy(i2cCnt);

		// Select device and start.
		*i2cData = devAddr;
		*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_START;
		i2cWaitBusy(i2cCnt);
		if(!I2C_GET_ACK(*i2cCnt)) // If ack flag is 0 it failed.
		{
			*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
			continue;
		}

		// Select register and change direction to write.
		*i2cData = regAddr;
		*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_WRITE;
		i2cWaitBusy(i2cCnt);
		if(!I2C_GET_ACK(*i2cCnt)) // If ack flag is 0 it failed.
		{
			*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
			continue;
		}

		// Select device in read mode for read transfer.
		if(read)
		{
			*i2cData = devAddr | 1u; // Set bit 0 for read.
			*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_START;
			i2cWaitBusy(i2cCnt);
			if(!I2C_GET_ACK(*i2cCnt)) // If ack flag is 0 it failed.
			{
				*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
				continue;
			}
		}

		break;
	}

	if(i < 8) return true;
	else return false;
}

void I2C_init(void)
{
	i2cWaitBusy(i2cGetBusRegsBase(0));
	REG_I2C1_CNTEX = 2;  // ?
	REG_I2C1_SCL = 1280; // ?

	i2cWaitBusy(i2cGetBusRegsBase(1));
	REG_I2C2_CNTEX = 2;  // ?
	REG_I2C2_SCL = 1280; // ?

	i2cWaitBusy(i2cGetBusRegsBase(2));
	REG_I2C3_CNTEX = 2;  // ?
	REG_I2C3_SCL = 1280; // ?
}

bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
	const u8 busId = i2cDevTable[devId].busId;
	vu8 *const i2cData = i2cGetBusRegsBase(busId);
	vu8 *const i2cCnt  = i2cData + 1;


	if(!i2cStartTransfer(devId, regAddr, true, i2cData)) return false;

	while(--size)
	{
		*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_READ | I2C_ACK;
		i2cWaitBusy(i2cCnt);
		*out++ = *i2cData;
	}

	*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_READ | I2C_STOP;
	i2cWaitBusy(i2cCnt);
	*out = *i2cData; // Last byte

	return true;
}

bool I2C_writeReg(I2cDevice devId, u8 regAddr, u8 data)
{
	const u8 busId = i2cDevTable[devId].busId;
	vu8 *const i2cData = i2cGetBusRegsBase(busId);
	vu8 *const i2cCnt  = i2cData + 1;


	if(!i2cStartTransfer(devId, regAddr, false, i2cData)) return false;

	*i2cData = data;
	*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_WRITE | I2C_STOP;
	i2cWaitBusy(i2cCnt);

	if(!I2C_GET_ACK(*i2cCnt)) // If ack flag is 0 it failed.
	{
		*i2cCnt = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
		return false;
	}

	return true;
}
