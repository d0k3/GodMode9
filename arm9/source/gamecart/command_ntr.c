// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// modifyed by osilloscopion (2 Jul 2016)
//

#include "command_ntr.h"
#include "protocol_ntr.h"
#include "card_ntr.h"
#include "delay.h"


u32 ReadDataFlags = 0;

void NTR_CmdReset(void)
{
    cardReset ();
    ioDelay2(0xF000);
}

u32 NTR_CmdGetCartId(void)
{
    return cardReadID (0);
}

void NTR_CmdEnter16ByteMode(void)
{
    static const u32 enter16bytemode_cmd[2] = { 0x3E000000, 0x00000000 };
    NTR_SendCommand(enter16bytemode_cmd, 0x0, 0, NULL);
}

void NTR_CmdReadHeader (u8* buffer)
{
	REG_NTRCARDROMCNT=0;
	REG_NTRCARDMCNT=0;
	ioDelay2(167550);
	REG_NTRCARDMCNT=NTRCARD_CR1_ENABLE|NTRCARD_CR1_IRQ;
	REG_NTRCARDROMCNT=NTRCARD_nRESET|NTRCARD_SEC_SEED;
	while(REG_NTRCARDROMCNT&NTRCARD_BUSY) ;
	cardReset();
	while(REG_NTRCARDROMCNT&NTRCARD_BUSY) ;
	u32 iCardId=cardReadID(NTRCARD_CLK_SLOW);
	while(REG_NTRCARDROMCNT&NTRCARD_BUSY) ;
	
	u32 iCheapCard=iCardId&0x80000000;
	
    if(iCheapCard)
    {
      //this is magic of wood goblins
      for(size_t ii=0;ii<8;++ii)
        cardParamCommand(NTRCARD_CMD_HEADER_READ,ii*0x200,NTRCARD_ACTIVATE|NTRCARD_nRESET|NTRCARD_CLK_SLOW|NTRCARD_BLK_SIZE(1)|NTRCARD_DELAY1(0x1FFF)|NTRCARD_DELAY2(0x3F),(u32*)(void*)(buffer+ii*0x200),0x200/sizeof(u32));
    }
    else
    {
      //0xac3f1fff
      cardParamCommand(NTRCARD_CMD_HEADER_READ,0,NTRCARD_ACTIVATE|NTRCARD_nRESET|NTRCARD_CLK_SLOW|NTRCARD_BLK_SIZE(4)|NTRCARD_DELAY1(0x1FFF)|NTRCARD_DELAY2(0x3F),(u32*)(void*)buffer,0x1000/sizeof(u32));
    }
    //cardReadHeader (buffer);
}

void NTR_CmdReadData (u32 offset, void* buffer)
{
    cardParamCommand (NTRCARD_CMD_DATA_READ, offset, ReadDataFlags | NTRCARD_ACTIVATE | NTRCARD_nRESET | NTRCARD_BLK_SIZE(1), (u32*)buffer, 0x200 / 4);
}


