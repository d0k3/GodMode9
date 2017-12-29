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
#include "spiflash.h"


#define SPI_REGS_BUS2_BASE  (0x10000000 + 0x100000 + 0x60000)
#define REG_SPI_BUS2_CNT    *((vu16*)(SPI_REGS_BUS2_BASE + 0x00))
#define REG_SPI_BUS2_DATA   *((vu8* )(SPI_REGS_BUS2_BASE + 0x02))



static void spi_busy_wait()
{
    while(REG_SPI_BUS2_CNT & 0x80);
}

static void spi_put_byte(u8 data)
{
    REG_SPI_BUS2_DATA = data;
    spi_busy_wait();
}

static u8 spi_receive_byte()
{
    // clock out a dummy byte
    REG_SPI_BUS2_DATA = 0x00;
    spi_busy_wait();
    return REG_SPI_BUS2_DATA;
}

// select spiflash if select=true, deselect otherwise 
static void spiflash_select(bool select)
{
    // select device 1, enable SPI bus
    REG_SPI_BUS2_CNT = 0x8100 | (select << 11);
}

bool spiflash_get_status()
{
    u8 resp;

    spi_busy_wait();
    spiflash_select(1);
    spi_put_byte(SPIFLASH_CMD_RDSR);
    spiflash_select(0);
    resp = spi_receive_byte();

    if(resp & 1) return false;
    return true;
}

void spiflash_read(u32 offset, u32 size, u8 *buf)
{
	spi_busy_wait();
	spiflash_select(1);
	spi_put_byte(SPIFLASH_CMD_READ);
	
	// write addr (24-bit, msb first)	
	for(int i=0; i<3; i++)
	{
		offset <<= 8;
		spi_put_byte((offset >> 24) & 0xFF);
	}
	
	// read bytes
	for(u32 i=0; i<size; i++)
		buf[i] = spi_receive_byte();
	
	// end of read
	spiflash_select(0);
	spi_receive_byte();
}
