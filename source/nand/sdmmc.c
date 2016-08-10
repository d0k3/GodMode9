/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2014-2015, Normmatt
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License Version 2, as described below:
 *
 * This file is free software: you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "sdmmc.h"
//#include "DrawCharacter.h"

#define DATA32_SUPPORT

#define TRUE 1
#define FALSE 0

#define NO_INLINE __attribute__ ((noinline))

#ifdef __cplusplus
extern "C" {
#endif
	void waitcycles(uint32_t val);
#ifdef __cplusplus
};
#endif

struct mmcdevice handelNAND;
struct mmcdevice handelSD;

mmcdevice *getMMCDevice(int drive)
{
	if(drive==0) return &handelNAND;
	return &handelSD;
}

static int geterror(struct mmcdevice *ctx)
{
	return (int)((ctx->error << 29) >> 31);
}


static void inittarget(struct mmcdevice *ctx)
{
	sdmmc_mask16(REG_SDPORTSEL,0x3,(uint16_t)ctx->devicenumber);
	setckl(ctx->clk);
	if(ctx->SDOPT == 0)
	{
		sdmmc_mask16(REG_SDOPT,0,0x8000);
	}
	else
	{
		sdmmc_mask16(REG_SDOPT,0x8000,0);
	}

}

static void NO_INLINE sdmmc_send_command(struct mmcdevice *ctx, uint32_t cmd, uint32_t args)
{
	uint32_t getSDRESP = (cmd << 15) >> 31;
	uint16_t flags = (cmd << 15) >> 31;
	const int readdata = cmd & 0x20000;
	const int writedata = cmd & 0x40000;

	if(readdata || writedata)
	{
		flags |= TMIO_STAT0_DATAEND;
	}

	ctx->error = 0;
	while((sdmmc_read16(REG_SDSTATUS1) & TMIO_STAT1_CMD_BUSY)); //mmc working?
	sdmmc_write16(REG_SDIRMASK0,0);
	sdmmc_write16(REG_SDIRMASK1,0);
	sdmmc_write16(REG_SDSTATUS0,0);
	sdmmc_write16(REG_SDSTATUS1,0);
	sdmmc_mask16(REG_DATACTL32,0x1800,0);
	sdmmc_write16(REG_SDCMDARG0,args &0xFFFF);
	sdmmc_write16(REG_SDCMDARG1,args >> 16);
	sdmmc_write16(REG_SDCMD,cmd &0xFFFF);

	uint32_t size = ctx->size;
	uint8_t *rDataPtr = ctx->rData;
	const uint8_t *tDataPtr = ctx->tData;

	int rUseBuf = ( NULL != rDataPtr );
	int tUseBuf = ( NULL != tDataPtr );

	uint16_t status0 = 0;
	while(1)
	{
		volatile uint16_t status1 = sdmmc_read16(REG_SDSTATUS1);
#ifdef DATA32_SUPPORT
		volatile uint16_t ctl32 = sdmmc_read16(REG_DATACTL32);
		if((ctl32 & 0x100))
#else
		if((status1 & TMIO_STAT1_RXRDY))
#endif
		{
			if(readdata)
			{
				if(rUseBuf)
				{
					sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_RXRDY, 0);
					if(size > 0x1FF)
					{
						#ifdef DATA32_SUPPORT
						//Gabriel Marcano: This implementation doesn't assume alignment.
						//I've removed the alignment check doen with former rUseBuf32 as a result
						for(int i = 0; i<0x200; i+=4)
						{
							uint32_t data = sdmmc_read32(REG_SDFIFO32);
							*rDataPtr++ = data;
							*rDataPtr++ = data >> 8;
							*rDataPtr++ = data >> 16;
							*rDataPtr++ = data >> 24;
						}
						#else
						for(int i = 0; i<0x200; i+=2)
						{
							uint16_t data = sdmmc_read16(REG_SDFIFO);
							*rDataPtr++ = data;
							*rDataPtr++ = data >> 8;
						}
						#endif
						size -= 0x200;
					}
				}

				sdmmc_mask16(REG_DATACTL32, 0x800, 0);
			}
		}
#ifdef DATA32_SUPPORT
		if(!(ctl32 & 0x200))
#else
		if((status1 & TMIO_STAT1_TXRQ))
#endif
		{
			if(writedata)
			{
				if(tUseBuf)
				{
					sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_TXRQ, 0);
					if(size > 0x1FF)
					{
						#ifdef DATA32_SUPPORT
						for(int i = 0; i<0x200; i+=4)
						{
							uint32_t data = *tDataPtr++;
							data |= (uint32_t)*tDataPtr++ << 8;
							data |= (uint32_t)*tDataPtr++ << 16;
							data |= (uint32_t)*tDataPtr++ << 24;
							sdmmc_write32(REG_SDFIFO32, data);
						}
						#else
						for(int i = 0; i<0x200; i+=2)
						{
							uint16_t data = *tDataPtr++;
							data |= (uint8_t)(*tDataPtr++ << 8);
							sdmmc_write16(REG_SDFIFO, data);
						}
						#endif
						size -= 0x200;
					}
				}

				sdmmc_mask16(REG_DATACTL32, 0x1000, 0);
			}
		}
		if(status1 & TMIO_MASK_GW)
		{
			ctx->error |= 4;
			break;
		}

		if(!(status1 & TMIO_STAT1_CMD_BUSY))
		{
			status0 = sdmmc_read16(REG_SDSTATUS0);
			if(sdmmc_read16(REG_SDSTATUS0) & TMIO_STAT0_CMDRESPEND)
			{
				ctx->error |= 0x1;
			}
			if(status0 & TMIO_STAT0_DATAEND)
			{
				ctx->error |= 0x2;
			}

			if((status0 & flags) == flags)
				break;
		}
	}
	ctx->stat0 = sdmmc_read16(REG_SDSTATUS0);
	ctx->stat1 = sdmmc_read16(REG_SDSTATUS1);
	sdmmc_write16(REG_SDSTATUS0,0);
	sdmmc_write16(REG_SDSTATUS1,0);

	if(getSDRESP != 0)
	{
		ctx->ret[0] = (uint32_t)(sdmmc_read16(REG_SDRESP0) | (sdmmc_read16(REG_SDRESP1) << 16));
		ctx->ret[1] = (uint32_t)(sdmmc_read16(REG_SDRESP2) | (sdmmc_read16(REG_SDRESP3) << 16));
		ctx->ret[2] = (uint32_t)(sdmmc_read16(REG_SDRESP4) | (sdmmc_read16(REG_SDRESP5) << 16));
		ctx->ret[3] = (uint32_t)(sdmmc_read16(REG_SDRESP6) | (sdmmc_read16(REG_SDRESP7) << 16));
	}
}

int NO_INLINE sdmmc_sdcard_writesectors(uint32_t sector_no, uint32_t numsectors, const uint8_t *in)
{
	if(handelSD.isSDHC == 0) sector_no <<= 9;
	inittarget(&handelSD);
	sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
	sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
	sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
	sdmmc_write16(REG_SDBLKCOUNT,numsectors);
	handelSD.tData = in;
	handelSD.size = numsectors << 9;
	sdmmc_send_command(&handelSD,0x52C19,sector_no);
	return geterror(&handelSD);
}

int NO_INLINE sdmmc_sdcard_readsectors(uint32_t sector_no, uint32_t numsectors, uint8_t *out)
{
	if(handelSD.isSDHC == 0) sector_no <<= 9;
	inittarget(&handelSD);
	sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
	sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
	sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
	sdmmc_write16(REG_SDBLKCOUNT,numsectors);
	handelSD.rData = out;
	handelSD.size = numsectors << 9;
	sdmmc_send_command(&handelSD,0x33C12,sector_no);
	return geterror(&handelSD);
}



int NO_INLINE sdmmc_nand_readsectors(uint32_t sector_no, uint32_t numsectors, uint8_t *out)
{
	if(handelNAND.isSDHC == 0) sector_no <<= 9;
	inittarget(&handelNAND);
	sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
	sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
	sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
	sdmmc_write16(REG_SDBLKCOUNT,numsectors);
	handelNAND.rData = out;
	handelNAND.size = numsectors << 9;
	sdmmc_send_command(&handelNAND,0x33C12,sector_no);
	return geterror(&handelNAND);
}

int NO_INLINE sdmmc_nand_writesectors(uint32_t sector_no, uint32_t numsectors, const uint8_t *in) //experimental
{
	if(handelNAND.isSDHC == 0) sector_no <<= 9;
	inittarget(&handelNAND);
	sdmmc_write16(REG_SDSTOP,0x100);
#ifdef DATA32_SUPPORT
	sdmmc_write16(REG_SDBLKCOUNT32,numsectors);
	sdmmc_write16(REG_SDBLKLEN32,0x200);
#endif
	sdmmc_write16(REG_SDBLKCOUNT,numsectors);
	handelNAND.tData = in;
	handelNAND.size = numsectors << 9;
	sdmmc_send_command(&handelNAND,0x52C19,sector_no);
	return geterror(&handelNAND);
}

static uint32_t calcSDSize(uint8_t* csd, int type)
{
  uint32_t result = 0;
  if(type == -1) type = csd[14] >> 6;
  switch(type)
  {
    case 0:
      {
        uint32_t block_len=csd[9]&0xf;
        block_len=1u<<block_len;
        uint32_t mult=( uint32_t)((csd[4]>>7)|((csd[5]&3)<<1));
        mult=1u<<(mult+2);
        result=csd[8]&3;
        result=(result<<8)|csd[7];
        result=(result<<2)|(csd[6]>>6);
        result=(result+1)*mult*block_len/512;
      }
      break;
    case 1:
      result=csd[7]&0x3f;
      result=(result<<8)|csd[6];
      result=(result<<8)|csd[5];
      result=(result+1)*1024;
      break;
    default:
      break; //Do nothing otherwise FIXME perhaps return some error?
  }
  return result;
}

void InitSD()
{
	//sdmmc_mask16(0x100,0x800,0);
	//sdmmc_mask16(0x100,0x1000,0);
	//sdmmc_mask16(0x100,0x0,0x402);
	//sdmmc_mask16(0xD8,0x22,0x2);
	//sdmmc_mask16(0x100,0x2,0);
	//sdmmc_mask16(0xD8,0x22,0);
	//sdmmc_write16(0x104,0);
	//sdmmc_write16(0x108,1);
	//sdmmc_mask16(REG_SDRESET,1,0); //not in new Version -- nintendo's code does this
	//sdmmc_mask16(REG_SDRESET,0,1); //not in new Version -- nintendo's code does this
	//sdmmc_mask16(0x20,0,0x31D);
	//sdmmc_mask16(0x22,0,0x837F);
	//sdmmc_mask16(0xFC,0,0xDB);
	//sdmmc_mask16(0xFE,0,0xDB);
	////sdmmc_write16(REG_SDCLKCTL,0x20);
	////sdmmc_write16(REG_SDOPT,0x40EE);
	////sdmmc_mask16(0x02,0x3,0);
	//sdmmc_write16(REG_SDCLKCTL,0x40);
	//sdmmc_write16(REG_SDOPT,0x40EB);
	//sdmmc_mask16(0x02,0x3,0);
	//sdmmc_write16(REG_SDBLKLEN,0x200);
	//sdmmc_write16(REG_SDSTOP,0);

	*(volatile uint16_t*)0x10006100 &= 0xF7FFu; //SDDATACTL32
	*(volatile uint16_t*)0x10006100 &= 0xEFFFu; //SDDATACTL32
#ifdef DATA32_SUPPORT
	*(volatile uint16_t*)0x10006100 |= 0x402u; //SDDATACTL32
#else
	*(volatile uint16_t*)0x10006100 |= 0x402u; //SDDATACTL32
#endif
	*(volatile uint16_t*)0x100060D8 = (*(volatile uint16_t*)0x100060D8 & 0xFFDD) | 2;
#ifdef DATA32_SUPPORT
	*(volatile uint16_t*)0x10006100 &= 0xFFFFu; //SDDATACTL32
	*(volatile uint16_t*)0x100060D8 &= 0xFFDFu; //SDDATACTL
	*(volatile uint16_t*)0x10006104 = 512; //SDBLKLEN32
#else
	*(volatile uint16_t*)0x10006100 &= 0xFFFDu; //SDDATACTL32
	*(volatile uint16_t*)0x100060D8 &= 0xFFDDu; //SDDATACTL
	*(volatile uint16_t*)0x10006104 = 0; //SDBLKLEN32
#endif
	*(volatile uint16_t*)0x10006108 = 1; //SDBLKCOUNT32
	*(volatile uint16_t*)0x100060E0 &= 0xFFFEu; //SDRESET
	*(volatile uint16_t*)0x100060E0 |= 1u; //SDRESET
	*(volatile uint16_t*)0x10006020 |= TMIO_MASK_ALL; //SDIR_MASK0
	*(volatile uint16_t*)0x10006022 |= TMIO_MASK_ALL>>16; //SDIR_MASK1
	*(volatile uint16_t*)0x100060FC |= 0xDBu; //SDCTL_RESERVED7
	*(volatile uint16_t*)0x100060FE |= 0xDBu; //SDCTL_RESERVED8
	*(volatile uint16_t*)0x10006002 &= 0xFFFCu; //SDPORTSEL
#ifdef DATA32_SUPPORT
	*(volatile uint16_t*)0x10006024 = 0x20;
	*(volatile uint16_t*)0x10006028 = 0x40EE;
#else
	*(volatile uint16_t*)0x10006024 = 0x40; //Nintendo sets this to 0x20
	*(volatile uint16_t*)0x10006028 = 0x40EB; //Nintendo sets this to 0x40EE
#endif
	*(volatile uint16_t*)0x10006002 &= 0xFFFCu; ////SDPORTSEL
	*(volatile uint16_t*)0x10006026 = 512; //SDBLKLEN
	*(volatile uint16_t*)0x10006008 = 0; //SDSTOP
}

int Nand_Init()
{
	//NAND
	handelNAND.isSDHC = 0;
	handelNAND.SDOPT = 0;
	handelNAND.res = 0;
	handelNAND.initarg = 1;
	handelNAND.clk = 0x80;
	handelNAND.devicenumber = 1;

	inittarget(&handelNAND);
	waitcycles(0xF000);

	sdmmc_send_command(&handelNAND,0,0);

	do
	{
		do
		{
			sdmmc_send_command(&handelNAND,0x10701,0x100000);
		} while ( !(handelNAND.error & 1) );
	}
	while((handelNAND.ret[0] & 0x80000000) == 0);

	sdmmc_send_command(&handelNAND,0x10602,0x0);
	if((handelNAND.error & 0x4))return -1;

	sdmmc_send_command(&handelNAND,0x10403,handelNAND.initarg << 0x10);
	if((handelNAND.error & 0x4))return -1;

	sdmmc_send_command(&handelNAND,0x10609,handelNAND.initarg << 0x10);
	if((handelNAND.error & 0x4))return -1;

	handelNAND.total_size = calcSDSize((uint8_t*)&handelNAND.ret[0],0);
	handelNAND.clk = 1;
	setckl(1);

	sdmmc_send_command(&handelNAND,0x10407,handelNAND.initarg << 0x10);
	if((handelNAND.error & 0x4))return -1;

	handelNAND.SDOPT = 1;

	sdmmc_send_command(&handelNAND,0x10506,0x3B70100);
	if((handelNAND.error & 0x4))return -1;

	sdmmc_send_command(&handelNAND,0x10506,0x3B90100);
	if((handelNAND.error & 0x4))return -1;

	sdmmc_send_command(&handelNAND,0x1040D,handelNAND.initarg << 0x10);
	if((handelNAND.error & 0x4))return -1;

	sdmmc_send_command(&handelNAND,0x10410,0x200);
	if((handelNAND.error & 0x4))return -1;

	handelNAND.clk |= 0x200;

	inittarget(&handelSD);

	return 0;
}

int SD_Init()
{
	//SD
	handelSD.isSDHC = 0;
	handelSD.SDOPT = 0;
	handelSD.res = 0;
	handelSD.initarg = 0;
	handelSD.clk = 0x80;
	handelSD.devicenumber = 0;

	inittarget(&handelSD);

	waitcycles(1u << 22); //Card needs a little bit of time to be detected, it seems FIXME test again to see what a good number is for the delay

	//If not inserted
	if (!(*((volatile uint16_t*)(SDMMC_BASE + REG_SDSTATUS0)) & TMIO_STAT0_SIGSTATE)) return 5;

	sdmmc_send_command(&handelSD,0,0);
	sdmmc_send_command(&handelSD,0x10408,0x1AA);
	uint32_t temp = (handelSD.error & 0x1) << 0x1E;

	uint32_t temp2 = 0;
	do
	{
		do
		{
			sdmmc_send_command(&handelSD,0x10437,handelSD.initarg << 0x10);
			sdmmc_send_command(&handelSD,0x10769,0x00FF8000 | temp);
			temp2 = 1;
		} while ( !(handelSD.error & 1) );
	}
	while((handelSD.ret[0] & 0x80000000) == 0);

	if(!((handelSD.ret[0] >> 30) & 1) || !temp)
		temp2 = 0;

	handelSD.isSDHC = temp2;

	sdmmc_send_command(&handelSD,0x10602,0);
	if((handelSD.error & 0x4)) return -1;

	sdmmc_send_command(&handelSD,0x10403,0);
	if((handelSD.error & 0x4)) return -2;
	handelSD.initarg = handelSD.ret[0] >> 0x10;

	sdmmc_send_command(&handelSD,0x10609,handelSD.initarg << 0x10);
	if((handelSD.error & 0x4)) return -3;

	handelSD.total_size = calcSDSize((uint8_t*)&handelSD.ret[0],-1);
	handelSD.clk = 1;
	setckl(1);

	sdmmc_send_command(&handelSD,0x10507,handelSD.initarg << 0x10);
	if((handelSD.error & 0x4)) return -4;

	sdmmc_send_command(&handelSD,0x10437,handelSD.initarg << 0x10);
	if((handelSD.error & 0x4)) return -5;

	handelSD.SDOPT = 1;
	sdmmc_send_command(&handelSD,0x10446,0x2);
	if((handelSD.error & 0x4)) return -6;

	sdmmc_send_command(&handelSD,0x1040D,handelSD.initarg << 0x10);
	if((handelSD.error & 0x4)) return -7;

	sdmmc_send_command(&handelSD,0x10410,0x200);
	if((handelSD.error & 0x4)) return -8;
	handelSD.clk |= 0x200;

	return 0;
}

int sdmmc_get_cid(bool isNand, uint32_t *info)
{
	struct mmcdevice *device;
	if(isNand)
		device = &handelNAND;
	else
		device = &handelSD;

	inittarget(device);
	// use cmd7 to put sd card in standby mode
	// CMD7
	{
		sdmmc_send_command(device,0x10507,0);
		//if((device->error & 0x4)) return -1;
	}

	// get sd card info
	// use cmd10 to read CID
	{
		sdmmc_send_command(device,0x1060A,device->initarg << 0x10);
		//if((device->error & 0x4)) return -2;

		for( int i = 0; i < 4; ++i ) {
			info[i] = device->ret[i];
		}
	}

	// put sd card back to transfer mode
	// CMD7
	{
		sdmmc_send_command(device,0x10507,device->initarg << 0x10);
		//if((device->error & 0x4)) return -3;
	}

	return 0;
}

int sdmmc_sdcard_init()
{
	InitSD();
	int nand_res = Nand_Init();
	int sd_res = SD_Init();
	return nand_res | sd_res;
}
