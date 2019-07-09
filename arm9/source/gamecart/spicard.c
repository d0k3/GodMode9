/*
 *   This file is part of fastboot 3DS
 *   Copyright (C) 2019 derrek, profi200
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

#include "types.h"
#include "spicard.h"
#include "delay.h"

#define REG_CFG9_CARDCTL      *((vu16*)0x1000000C)

#define SPICARD_REGS_BASE  0x1000D800
#define REG_NSPI_CNT       *((vu32*)(SPICARD_REGS_BASE + 0x00))
#define REG_NSPI_DONE      *((vu32*)(SPICARD_REGS_BASE + 0x04))
#define REG_NSPI_BLKLEN    *((vu32*)(SPICARD_REGS_BASE + 0x08))
#define REG_NSPI_FIFO      *((vu32*)(SPICARD_REGS_BASE + 0x0C))
#define REG_NSPI_STATUS    *((vu32*)(SPICARD_REGS_BASE + 0x10))
#define REG_NSPI_AUTOPOLL  *((vu32*)(SPICARD_REGS_BASE + 0x14))
#define REG_NSPI_INT_MASK  *((vu32*)(SPICARD_REGS_BASE + 0x18))
#define REG_NSPI_INT_STAT  *((vu32*)(SPICARD_REGS_BASE + 0x1C))



static inline void nspiWaitBusy(void)
{
	while(REG_NSPI_CNT & NSPI_CNT_ENABLE);
}

static inline void nspiWaitFifoBusy(void)
{
	while(REG_NSPI_STATUS & NSPI_STATUS_BUSY);
}

void SPICARD_init(void)
{
	REG_CFG9_CARDCTL |= 1u<<8;
}

void SPICARD_deinit(void)
{
	REG_CFG9_CARDCTL &= ~(1u<<8);
}

/*
bool _SPICARD_autoPollBit(u32 params)
{
	REG_NSPI_AUTOPOLL = NSPI_AUTOPOLL_START | params;

	u32 res;
	do
	{
		__wfi();
		res = REG_NSPI_INT_STAT;
	} while(!(res & (NSPI_INT_AP_TIMEOUT | NSPI_INT_AP_SUCCESS)));
	REG_NSPI_INT_STAT = res; // Aknowledge

	return (res & NSPI_INT_AP_TIMEOUT) == 0; // Timeout error
}
*/

void SPICARD_writeRead(NspiClk clk, const void *in, void *out, u32 inSize, u32 outSize, bool done)
{
	const u32 cntParams = NSPI_CNT_ENABLE | NSPI_CNT_BUS_1BIT | clk;

	REG_CFG9_CARDCTL |= 1u<<8;

	u32 buf;
	char *in_ = (char *) in;
	char *out_ = (char *) out;

	if(in_)
	{
		REG_NSPI_BLKLEN = inSize;
		REG_NSPI_CNT = cntParams | NSPI_CNT_DIRE_WRITE;

		u32 counter = 0;
		do
		{
			if((counter & 31) == 0) nspiWaitFifoBusy();
			memcpy(&buf, in_, min(4, inSize - counter));
			REG_NSPI_FIFO = buf;
			counter += 4;
			in_ += 4;
		} while(counter < inSize);

		nspiWaitBusy();
	}
	if(out_)
	{
		REG_NSPI_BLKLEN = outSize;
		REG_NSPI_CNT = cntParams | NSPI_CNT_DIRE_READ;

		u32 counter = 0;
		do
		{
			if((counter & 31) == 0) nspiWaitFifoBusy();
			buf = REG_NSPI_FIFO;
			memcpy(out_, &buf, min(4, outSize - counter));
			counter += 4;
			out_ += 4;
		} while(counter < outSize);

		nspiWaitBusy();
	}

	if(done) REG_NSPI_DONE = NSPI_DONE;
}
