/*---------------------------------------------------------------------------------

	Copyright (C) 2005
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.


---------------------------------------------------------------------------------*/
#include "ndscard.h"

//---------------------------------------------------------------------------------
void cardWriteCommand(const u8 *command) {
//---------------------------------------------------------------------------------
	int index;

	REG_AUXSPICNT = CARD_CR1_ENABLE | CARD_CR1_IRQ;

	for (index = 0; index < 8; index++) {
		CARD_COMMAND[7-index] = command[index];
	}
}


//---------------------------------------------------------------------------------
void cardPolledTransfer(u32 flags, u32 *destination, u32 length, const u8 *command) {
//---------------------------------------------------------------------------------
	u32 data;
	cardWriteCommand(command);
	REG_ROMCTRL = flags;
	u32 * target = destination + length;
	do {
		// Read data if available
		if (REG_ROMCTRL & CARD_DATA_READY) {
			data=CARD_DATA_RD;
			if (destination < target)
				*destination = data;
			destination++;
		}
	} while (REG_ROMCTRL & CARD_BUSY);
}


//---------------------------------------------------------------------------------
void cardStartTransfer(const u8 *command, u32 *destination, int channel, u32 flags) {
//---------------------------------------------------------------------------------
	cardWriteCommand(command);

	// Set up a DMA channel to transfer a word every time the card makes one
	DMA_SRC(channel) = (u32)&CARD_DATA_RD;
	DMA_DEST(channel) = (u32)destination;
	DMA_CR(channel) = DMA_ENABLE | DMA_START_CARD | DMA_32_BIT | DMA_REPEAT | DMA_SRC_FIX | 0x0001;

	REG_ROMCTRL = flags;
}


//---------------------------------------------------------------------------------
u32 cardWriteAndRead(const u8 *command, u32 flags) {
//---------------------------------------------------------------------------------
	cardWriteCommand(command);
	REG_ROMCTRL = flags | CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(7);
	while (!(REG_ROMCTRL & CARD_DATA_READY)) ;
	return CARD_DATA_RD;
}

//---------------------------------------------------------------------------------
void cardParamCommand (u8 command, u32 parameter, u32 flags, u32 *destination, u32 length) {
//---------------------------------------------------------------------------------
	u8 cmdData[8];

	cmdData[7] = (u8) command;
	cmdData[6] = (u8) (parameter >> 24);
	cmdData[5] = (u8) (parameter >> 16);
	cmdData[4] = (u8) (parameter >>  8);
	cmdData[3] = (u8) (parameter >>  0);
	cmdData[2] = 0;
	cmdData[1] = 0;
	cmdData[0] = 0;

	cardPolledTransfer(flags, destination, length, cmdData);
}

//---------------------------------------------------------------------------------
void cardReadHeader(u8 *header) {
//---------------------------------------------------------------------------------
	REG_ROMCTRL=0;
	REG_AUXSPICNT=0;
	swiDelay(167550);
	REG_AUXSPICNT=CARD_CR1_ENABLE|CARD_CR1_IRQ;
	REG_ROMCTRL=CARD_nRESET|CARD_SEC_SEED;
	while(REG_ROMCTRL&CARD_BUSY) ;
	cardReset();
	while(REG_ROMCTRL&CARD_BUSY) ;

	cardParamCommand(CARD_CMD_HEADER_READ,0,CARD_ACTIVATE|CARD_nRESET|CARD_CLK_SLOW|CARD_BLK_SIZE(1)|CARD_DELAY1(0x1FFF)|CARD_DELAY2(0x3F),(u32*)(void*)header,512/4);
}


//---------------------------------------------------------------------------------
u32 cardReadID(u32 flags) {
//---------------------------------------------------------------------------------
	const u8 command[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, CARD_CMD_HEADER_CHIPID};
	return cardWriteAndRead(command, flags);
}


//---------------------------------------------------------------------------------
void cardReset() {
//---------------------------------------------------------------------------------
	const u8 cmdData[8]={0,0,0,0,0,0,0,CARD_CMD_DUMMY};
	cardWriteCommand(cmdData);
	REG_ROMCTRL=CARD_ACTIVATE|CARD_nRESET|CARD_CLK_SLOW|CARD_BLK_SIZE(5)|CARD_DELAY2(0x18);
	u32 read=0;

	do {
		if(REG_ROMCTRL&CARD_DATA_READY) {
			if(read<0x2000) {
				u32 data=CARD_DATA_RD;
				(void)data;
				read+=4;
			}
		}
	} while(REG_ROMCTRL&CARD_BUSY);
}
