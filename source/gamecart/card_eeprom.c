/*---------------------------------------------------------------------------------

	Copyright (C) 2005 - 2010
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
#include "card_eeprom.h"

//---------------------------------------------------------------------------------
u8 cardEepromCommand(u8 command) {
//---------------------------------------------------------------------------------
	u8 retval;

	REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;

	REG_AUXSPIDATA = command;

	eepromWaitBusy();

	REG_AUXSPIDATA = 0;
	eepromWaitBusy();
	retval = REG_AUXSPIDATA;
	REG_AUXSPICNT = /*MODE*/0x40;
	return retval;
}


//---------------------------------------------------------------------------------
u32 cardEepromReadID() {
//---------------------------------------------------------------------------------
	int i;

	REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;

	REG_AUXSPIDATA = SPI_EEPROM_RDID;

	eepromWaitBusy();
	u32 id = 0;
	for ( i=0; i<3; i++) {
		REG_AUXSPIDATA = 0;
		eepromWaitBusy();
		id = (id << 8) | REG_AUXSPIDATA;
	}

	REG_AUXSPICNT = /*MODE*/0x40;

	return id;
}


//---------------------------------------------------------------------------------
int cardEepromGetType(void) {
//---------------------------------------------------------------------------------
	int sr = cardEepromCommand(SPI_EEPROM_RDSR);
	int id = cardEepromReadID();

	if (( sr == 0xff && id == 0xffffff) || ( sr == 0 && id == 0 )) return -1;
	if ( sr == 0xf0 && id == 0xffffff ) return 1;
	if ( sr == 0x00 && id == 0xffffff ) return 2;
	if ( id != 0xffffff) return 3;

	return 0;
}

//---------------------------------------------------------------------------------
u32 cardEepromGetSize() {
//---------------------------------------------------------------------------------

	int type = cardEepromGetType();

	if(type == -1)
		return 0;
	if(type == 0)
		return 8192;
	if(type == 1)
		return 512;
	if(type == 2) {
		u32 buf1,buf2,buf3;
		cardReadEeprom(0,(u8*)&buf1,4,type);
		if ( !(buf1 != 0 || buf1 != 0xffffffff) ) {
			buf3 = ~buf1;
			cardWriteEeprom(0,(u8*)&buf3,4,type);
		} else {
			buf3 = buf1;
		}
		int size = 8192;
		while (1) {
			cardReadEeprom(size,(u8*)&buf2,4,type);
			if ( buf2 == buf3 ) break;
			size += 8192;
		};

		if ( buf1 != buf3 ) cardWriteEeprom(0,(u8*)&buf1,4,type);

		return size;
	}

	int device;

	if(type == 3) {
		int id = cardEepromReadID();

		device = id & 0xffff;

		if ( ((id >> 16) & 0xff) == 0x20 ) { // ST

			switch(device) {

			case 0x4014:
				return 1024*1024;		//	8Mbit(1 meg)
				break;
			case 0x4013:
			case 0x8013:				// M25PE40
				return 512*1024;		//	4Mbit(512KByte)
				break;
			case 0x2017:
				return 8*1024*1024;		//	64Mbit(8 meg)
				break;
			}
		}

		if ( ((id >> 16) & 0xff) == 0x62 ) { // Sanyo

			if (device == 0x1100)
				return 512*1024;		//	4Mbit(512KByte)

		}

		if ( ((id >> 16) & 0xff) == 0xC2 ) { // Macronix

			if (device == 0x2211)
				return 128*1024;		//	1Mbit(128KByte) - MX25L1021E
		}


		return 256*1024;		//	2Mbit(256KByte)
	}

	return 0;
}


//---------------------------------------------------------------------------------
void cardReadEeprom(u32 address, u8 *data, u32 length, u32 addrtype) {
//---------------------------------------------------------------------------------

	REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;
	REG_AUXSPIDATA = 0x03 | ((addrtype == 1) ? address>>8<<3 : 0);
	eepromWaitBusy();

	if (addrtype == 3) {
		REG_AUXSPIDATA = (address >> 16) & 0xFF;
		eepromWaitBusy();
	}

	if (addrtype >= 2) {
		REG_AUXSPIDATA = (address >> 8) & 0xFF;
		eepromWaitBusy();
	}


	REG_AUXSPIDATA = (address) & 0xFF;
	eepromWaitBusy();

	while (length > 0) {
		REG_AUXSPIDATA = 0;
		eepromWaitBusy();
		*data++ = REG_AUXSPIDATA;
		length--;
	}

	eepromWaitBusy();
	REG_AUXSPICNT = /*MODE*/0x40;
}


//---------------------------------------------------------------------------------
void cardWriteEeprom(u32 address, u8 *data, u32 length, u32 addrtype) {
//---------------------------------------------------------------------------------

	u32 address_end = address + length;
	int i;
	int maxblocks = 32;
	if(addrtype == 1) maxblocks = 16;
	if(addrtype == 2) maxblocks = 32;
	if(addrtype == 3) maxblocks = 256;

	while (address < address_end) {
		// set WEL (Write Enable Latch)
		REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;
		REG_AUXSPIDATA = 0x06; eepromWaitBusy();
		REG_AUXSPICNT = /*MODE*/0x40;

		// program maximum of 32 bytes
		REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;

		if(addrtype == 1) {
		//	WRITE COMMAND 0x02 + A8 << 3
			REG_AUXSPIDATA = 0x02 | (address & (1 << (8))) >> (8-3) ;
			eepromWaitBusy();
			REG_AUXSPIDATA = address & 0xFF;
			eepromWaitBusy();
		}
		else if(addrtype == 2) {
			REG_AUXSPIDATA = 0x02;
			eepromWaitBusy();
			REG_AUXSPIDATA = address >> 8;
			eepromWaitBusy();
			REG_AUXSPIDATA = address & 0xFF;
			eepromWaitBusy();
		}
		else if(addrtype == 3) {
			REG_AUXSPIDATA = 0x02;
			eepromWaitBusy();
			REG_AUXSPIDATA = (address >> 16) & 0xFF;
			eepromWaitBusy();
			REG_AUXSPIDATA = (address >> 8) & 0xFF;
			eepromWaitBusy();
			REG_AUXSPIDATA = address & 0xFF;
			eepromWaitBusy();
		}

		for (i=0; address<address_end && i<maxblocks; i++, address++) {
			REG_AUXSPIDATA = *data++;
			eepromWaitBusy();
		}
		REG_AUXSPICNT = /*MODE*/0x40;

		// wait programming to finish
		REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;
		REG_AUXSPIDATA = 0x05; eepromWaitBusy();
		do { REG_AUXSPIDATA = 0; eepromWaitBusy(); } while (REG_AUXSPIDATA & 0x01); // WIP (Write In Progress) ?
		eepromWaitBusy();
		REG_AUXSPICNT = /*MODE*/0x40;
	}
}

//---------------------------------------------------------------------------------
//	Chip Erase : clear FLASH MEMORY (TYPE 3 ONLY)
//---------------------------------------------------------------------------------
void cardEepromChipErase(void) {
//---------------------------------------------------------------------------------
	int sz, sector;
	sz=cardEepromGetSize();

	for ( sector = 0; sector < sz; sector+=0x10000) {
		cardEepromSectorErase(sector);
	}
}

//---------------------------------------------------------------------------------
//	COMMAND Sec.erase 0xD8
//---------------------------------------------------------------------------------
void cardEepromSectorErase(u32 address) {
//---------------------------------------------------------------------------------
		// set WEL (Write Enable Latch)
		REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;
		REG_AUXSPIDATA = 0x06;
		eepromWaitBusy();

		REG_AUXSPICNT = /*MODE*/0x40;

		// SectorErase 0xD8
		REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;
		REG_AUXSPIDATA = 0xD8;
		eepromWaitBusy();
		REG_AUXSPIDATA = (address >> 16) & 0xFF;
		eepromWaitBusy();
		REG_AUXSPIDATA = (address >> 8) & 0xFF;
		eepromWaitBusy();
		REG_AUXSPIDATA = address & 0xFF;
		eepromWaitBusy();

		REG_AUXSPICNT = /*MODE*/0x40;

		// wait erase to finish
		REG_AUXSPICNT = /*E*/0x8000 | /*SEL*/0x2000 | /*MODE*/0x40;
		REG_AUXSPIDATA = 0x05;
		eepromWaitBusy();

		do {
			REG_AUXSPIDATA = 0;
			eepromWaitBusy();
		} while (REG_AUXSPIDATA & 0x01);  // WIP (Write In Progress) ?
		REG_AUXSPICNT = /*MODE*/0x40;
}
