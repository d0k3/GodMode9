/*
 *  This file is based on SPI.cpp from TWLSaveTool. Its copyright notice is
 *  reproduced below.
 *
 *  Copyright (C) 2015-2016 TuxSH
 *
 *  TWLSaveTool is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "spi.h"
#include <spi.h>
#include "timer.h"

// declarations for actual implementations
int SPIEnableWriting_512B(CardType type);
int SPIEnableWriting_regular(CardType type);
int SPIReadSaveData_9bit(CardType type, u32 offset, void* data, u32 size);
int SPIReadSaveData_16bit(CardType type, u32 offset, void* data, u32 size);
int SPIReadSaveData_24bit(CardType type, u32 offset, void* data, u32 size);
int SPIWriteSaveData_9bit(CardType type, u32 offset, const void* data, u32 size);
int SPIWriteSaveData_16bit(CardType type, u32 offset, const void* data, u32 size);
int SPIWriteSaveData_24bit_write(CardType type, u32 offset, const void* data, u32 size);
int SPIWriteSaveData_24bit_erase_program(CardType type, u32 offset, const void* data, u32 size);
int SPIEraseSector_emulated(CardType type, u32 offset);
int SPIEraseSector_real(CardType type, u32 offset);

const CardTypeData EEPROM_512B_ = { SPIEnableWriting_512B, SPIReadSaveData_9bit, SPIWriteSaveData_9bit, SPIEraseSector_emulated, 0xffffff, 1 << 9, 16, 16, 16, false, 0, 0, 0 };

const CardTypeData EEPROM_STD_DUMMY = { SPIEnableWriting_regular, SPIReadSaveData_16bit, SPIWriteSaveData_16bit, SPIEraseSector_emulated, 0xffffff, UINT32_MAX, 1, 1, 1, false, SPI_EEPROM_CMD_WRITE, 0, 0 };
const CardTypeData EEPROMTypes[] = {
	{ SPIEnableWriting_regular, SPIReadSaveData_16bit, SPIWriteSaveData_16bit, SPIEraseSector_emulated, 0xffffff, 1 << 13, 32, 32, 32, false, SPI_EEPROM_CMD_WRITE, 0, 0}, // EEPROM 8 KB
	{ SPIEnableWriting_regular, SPIReadSaveData_16bit, SPIWriteSaveData_16bit, SPIEraseSector_emulated, 0xffffff, 1 << 16, 128, 128, 128, false, SPI_EEPROM_CMD_WRITE, 0, 0}, // EEPROM 64 KB
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_emulated, 0xffffff, 1 << 17, 256, 256, 256, false, SPI_EEPROM_CMD_WRITE, 0, 0}, // EEPROM 128 KB
};

const CardTypeData FLASH_STD_DUMMY = { NULL, SPIReadSaveData_24bit, NULL, NULL, 0x0, 0, 0, 0, 0, false, 0, 0, 0 };
const CardTypeData FlashStdTypes[] = {
	// NTR/TWL
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x204012, 1 << 18, 65536, 256, 256, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0x621600, 1 << 18, 65536, 256, 65536, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x204013, 1 << 19, 65536, 256, 256, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x621100, 1 << 19, 65536, 256, 256, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x204014, 1 << 20, 65536, 256, 256, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	// Untested (but pretty safe bet), for Art Academy
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0x202017, 1 << 23, 65536, 32, 65536, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0x204017, 1 << 23, 65536, 32, 65536, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	// CTR
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22210, 1 << 16, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22211, 1 << 17, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22212, 1 << 18, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22213, 1 << 19, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22214, 1 << 20, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22215, 1 << 21, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22216, 1 << 22, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0xC22217, 1 << 23, 4096, 32, 4096, false, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_MXIC_SE },
};

const CardTypeData FLASH_INFRARED_DUMMY = { NULL, SPIReadSaveData_24bit, NULL, NULL, 0x0, 0, 0, 0, 0, true, 0, 0, 0 };
const CardTypeData FlashInfraredTypes[] = {
	// NTR/TWL
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x204012, 1 << 18, 65536, 256, 256, true, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_erase_program, SPIEraseSector_real, 0x621600, 1 << 18, 65536, 256, 65536, true, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x204013, 1 << 19, 65536, 256, 256, true, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
	{ SPIEnableWriting_regular, SPIReadSaveData_24bit, SPIWriteSaveData_24bit_write, SPIEraseSector_real, 0x621100, 1 << 19, 65536, 256, 256, true, SPI_FLASH_CMD_PW, SPI_CMD_PP, SPI_FLASH_CMD_SE },
};

const CardType EEPROM_512B = &EEPROM_512B_;

const CardType EEPROM_8KB = EEPROMTypes + 0;
const CardType EEPROM_64KB = EEPROMTypes + 1;
const CardType EEPROM_128KB = EEPROMTypes + 2;

const CardType FLASH_256KB_1 = FlashStdTypes + 0;
const CardType FLASH_256KB_2 = FlashStdTypes + 1;
const CardType FLASH_512KB_1 = FlashStdTypes + 2;
const CardType FLASH_512KB_2 = FlashStdTypes + 3;
const CardType FLASH_1MB = FlashStdTypes + 4;
const CardType FLASH_8MB_1 = FlashStdTypes + 5;
const CardType FLASH_8MB_2 = FlashStdTypes + 6;

const CardType FLASH_64KB_CTR = FlashStdTypes + 7;
const CardType FLASH_128KB_CTR = FlashStdTypes + 8;
const CardType FLASH_256KB_CTR = FlashStdTypes + 9;
const CardType FLASH_512KB_CTR = FlashStdTypes + 10;
const CardType FLASH_1MB_CTR = FlashStdTypes + 11;
const CardType FLASH_2MB_CTR = FlashStdTypes + 12;
const CardType FLASH_4MB_CTR = FlashStdTypes + 13;
const CardType FLASH_8MB_CTR = FlashStdTypes + 14;

const CardType FLASH_256KB_1_INFRARED = FlashInfraredTypes + 0;
const CardType FLASH_256KB_2_INFRARED = FlashInfraredTypes + 1;
const CardType FLASH_512KB_1_INFRARED = FlashInfraredTypes + 2;
const CardType FLASH_512KB_2_INFRARED = FlashInfraredTypes + 3;

#define REG_CFG9_CARDCTL      *((vu16*)0x1000000C)
#define CARDCTL_SPICARD       (1u<<8)

int SPIWriteRead(CardType type, const void* cmd, u32 cmdSize, void* answer, u32 answerSize, const void* data, u32 dataSize) {
	u32 headerFooterVal = 0;

	REG_CFG9_CARDCTL |= CARDCTL_SPICARD;

	if (type->infrared) {
		SPI_XferInfo irXfer = { &headerFooterVal, 1, false };
		SPI_DoXfer(SPI_DEV_CART_IR, &irXfer, 1, false);
	}

	SPI_XferInfo transfers[3] = {
		{ (u8*) cmd, cmdSize, false },
		{ answer, answerSize, true },
		{ (u8*) data, dataSize, false },
	};
	SPI_DoXfer(SPI_DEV_CART_FLASH, transfers, 3, true);

	REG_CFG9_CARDCTL &= ~CARDCTL_SPICARD;
	
	return 0;
}

int SPIWaitWriteEnd(CardType type) {
	u8 cmd = SPI_CMD_RDSR, statusReg = 0;
	int res = 0;
	u64 time_start = timer_start();

	do{
		res = SPIWriteRead(type, &cmd, 1, &statusReg, 1, 0, 0);
		if(res) return res;
		if(timer_msec(time_start) > 1000) return 1;
	} while(statusReg & SPI_FLG_WIP);

	return 0;
}

int SPIEnableWriting_512B(CardType type) {
	u8 cmd = SPI_CMD_WREN;
	return SPIWriteRead(type, &cmd, 1, NULL, 0, 0, 0);
}

int SPIEnableWriting_regular(CardType type) {
	u8 cmd = SPI_CMD_WREN, statusReg = 0;
	int res = SPIWriteRead(type, &cmd, 1, NULL, 0, 0, 0);

	if(res) return res;
	cmd = SPI_CMD_RDSR;
	
	do{
		res = SPIWriteRead(type, &cmd, 1, &statusReg, 1, 0, 0);
		if(res) return res;
	} while(statusReg & ~SPI_FLG_WEL);
	
	return 0;
}

int SPIEnableWriting(CardType type) {
	if(type == NO_CHIP) return 1;
	return type->enableWriting(type);
}

int _SPIWriteTransaction(CardType type, void* cmd, u32 cmdSize, const void* data, u32 dataSize) {
	int res;
	if( (res = SPIEnableWriting(type)) ) return res;
	if( (res = SPIWriteRead(type, cmd, cmdSize, NULL, 0, (void*) ((u8*) data), dataSize)) ) return res;
	return SPIWaitWriteEnd(type);
}

int SPIReadJEDECIDAndStatusReg(CardType type, u32* id, u8* statusReg) {
	u8 cmd = SPI_FLASH_CMD_RDID;
	u8 reg = 0;
	u8 idbuf[3] = { 0 };
	u32 id_ = 0;
	int res = SPIWaitWriteEnd(type);
	if(res) return res;
	
	if((res = SPIWriteRead(type, &cmd, 1, idbuf, 3, 0, 0))) return res;
	
	id_ = (idbuf[0] << 16) | (idbuf[1] << 8) | idbuf[2];
	cmd = SPI_CMD_RDSR;
	
	if((res = SPIWriteRead(type, &cmd, 1, &reg, 1, 0, 0))) return res;
	
	if(id) *id = id_;
	if(statusReg) *statusReg = reg;
	
	return 0;
}

u32 SPIGetPageSize(CardType type) {
	if(type == NO_CHIP) return 0;
	return type->pageSize;
}

u32 SPIGetEraseSize(CardType type) {
	if(type == NO_CHIP) return 0;
	return type->eraseSize;
}

u32 SPIGetCapacity(CardType type) {
	if(type == NO_CHIP) return 0;
	return type->capacity;
}

int SPIWriteSaveData_9bit(CardType type, u32 offset, const void* data, u32 size) {
	u8 cmd[2] = { (offset >= 0x100) ? SPI_512B_EEPROM_CMD_WRHI : SPI_512B_EEPROM_CMD_WRLO, (u8) offset };
	
	return _SPIWriteTransaction(type, cmd, 2, (void*) ((u8*) data), size);
}

int SPIWriteSaveData_16bit(CardType type, u32 offset, const void* data, u32 size) {
	u8 cmd[3] = { type->writeCommand, (u8)(offset >> 8), (u8) offset };
	
	return _SPIWriteTransaction(type, cmd, 3, (void*) ((u8*) data), size);
}

int SPIWriteSaveData_24bit_write(CardType type, u32 offset, const void* data, u32 size) {
	u8 cmd[4] = { type->writeCommand, (u8)(offset >> 16), (u8)(offset >> 8), (u8) offset };
	
	return _SPIWriteTransaction(type, cmd, 4, (void*) ((u8*) data), size);
}

int SPIWriteSaveData_24bit_erase_program(CardType type, u32 offset, const void* data, u32 size) {
	u8 cmd[4] = { type->programCommand };
	const u32 pageSize = SPIGetPageSize(type);
	const u32 eraseSize = SPIGetEraseSize(type);
	int res;

	u8 *newData = NULL;
	if(offset % eraseSize || size < eraseSize) {
		u32 sectorStart = (offset / eraseSize) * eraseSize;
		newData = malloc(eraseSize);
		if(!newData) return 1;
		if( (res = SPIReadSaveData(type, sectorStart, newData, eraseSize)) ) {
			free(newData);
			return res;
		}
		memcpy(newData + (offset % eraseSize), data, size);
		data = newData;
		offset = sectorStart;
	}

	if( (res = SPIEraseSector(type, offset)) ) {
		free(newData);
		return res;
	}

	for(u32 pos = offset; pos < offset + eraseSize; pos += pageSize) {
		cmd[1] = (u8)(pos >> 16);
		cmd[2] = (u8)(pos >> 8);
		cmd[3] = (u8) pos;
		if( (res = _SPIWriteTransaction(type, cmd, 4, (void*) ((u8*) data - offset + pos), pageSize)) ) {
			free(newData);
			return res;
		}
	}

	free(newData);
	return 0;
}

int SPIWriteSaveData(CardType type, u32 offset, const void* data, u32 size) {
	if(type == NO_CHIP) return 1;
	
	if(size == 0) return 0;
	size = min(size, SPIGetCapacity(type) - offset);
	u32 end = offset + size;
	u32 pos = offset;
	u32 writeSize = type->writeSize;
	if(writeSize == 0) return 0xC8E13404;
	
	int res = SPIWaitWriteEnd(type);
	if(res) return res;
	
	while(pos < end) {
		u32 remaining = end - pos;
		u32 nb = writeSize - (pos % writeSize);
		
		u32 dataSize = (remaining < nb) ? remaining : nb;
		
		if( (res = type->writeSaveData(type, pos, (void*) ((u8*) data - offset + pos), dataSize)) ) return res;
		
		pos = ((pos / writeSize) + 1) * writeSize; // truncate
	}
	
	return 0;
}

int SPIReadSaveData_9bit(CardType type, u32 pos, void* data, u32 size) { 
	u8 cmd[4];
	u32 cmdSize = 2;
	
	u32 end = pos + size;
	
	u32 read = 0;
	if(pos < 0x100) {
		u32 len = 0x100 - pos;
		cmd[0] = SPI_512B_EEPROM_CMD_RDLO;
		cmd[1] = (u8) pos;
		
		int res = SPIWriteRead(type, cmd, cmdSize, data, len, NULL, 0);
		if(res) return res;
		
		read += len;
	}
	
	if(end >= 0x100) {
		u32 len = end - 0x100;

		cmd[0] = SPI_512B_EEPROM_CMD_RDHI;
		cmd[1] = (u8)(pos + read);
		
		int res = SPIWriteRead(type, cmd, cmdSize, (void*)((u8*)data + read), len, NULL, 0);

		if(res) return res;
	}
	
	return 0;
}

int SPIReadSaveData_16bit(CardType type, u32 offset, void* data, u32 size) {	
	u8 cmd[3] = { SPI_CMD_READ, (u8)(offset >> 8), (u8) offset };

	return SPIWriteRead(type, cmd, 3, data, size, NULL, 0);
}

int SPIReadSaveData_24bit(CardType type, u32 offset, void* data, u32 size) {	
	u8 cmd[4] = { SPI_CMD_READ, (u8)(offset >> 16), (u8)(offset >> 8), (u8) offset };
	
	return SPIWriteRead(type, cmd, 4, data, size, NULL, 0);
}

int SPIReadSaveData(CardType type, u32 offset, void* data, u32 size) {
	if(type == NO_CHIP) return 1;

	if(size == 0) return 0;
	
	int res = SPIWaitWriteEnd(type);
	if(res) return res;
	
	size = (size <= SPIGetCapacity(type) - offset) ? size : SPIGetCapacity(type) - offset;

	return type->readSaveData(type, offset, data, size);
}

int SPIEraseSector_emulated(CardType type, u32 offset) {
	u32 blockSize = SPIGetEraseSize(type);
	u8 *fill_buf = malloc(SPIGetEraseSize(type));
	if (!fill_buf) return 1;
	memset(fill_buf, 0xff, blockSize);
	offset = (offset / blockSize) * blockSize;
	
	int res = SPIWriteSaveData(type, offset, fill_buf, blockSize);
	free(fill_buf);
	return res;
}

int SPIEraseSector_real(CardType type, u32 offset) {
	u8 cmd[4] = { type->eraseCommand, (u8)(offset >> 16), (u8)(offset >> 8), (u8) offset };
	
	int res = SPIWaitWriteEnd(type);
	if(res) return res;
	
	return _SPIWriteTransaction(type, cmd, 4, NULL, 0);
}


int SPIEraseSector(CardType type, u32 offset) {
	if(type == NO_CHIP) return 1;
	return type->eraseSector(type, offset);
}


// The following routine use code from savegame-manager:

/*
 * savegame_manager: a tool to backup and restore savegames from Nintendo
 *  DS cartridges. Nintendo DS and all derivative names are trademarks
 *  by Nintendo. EZFlash 3-in-1 is a trademark by EZFlash.
 *
 * auxspi.cpp: A thin reimplementation of the AUXSPI protocol
 *   (high level functions)
 *
 * Copyright (C) Pokedoc (2010)
 */
/* 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

 
int _SPIIsDataMirrored(CardType type, int size, bool* mirrored) {
	u32 offset0 = (size-1);        //      n KB
	u32 offset1 = (2*size-1);      //     2n KB
	
	u8 buf1;     //      +0k data        read -> write
	u8 buf2;     //      +n k data        read -> read
	u8 buf3;     //      +0k ~data          write
	u8 buf4;     //      +n k data new    comp buf2
	
	int res;
	
	if( (res = SPIReadSaveData(type, offset0, &buf1, 1)) ) return res;
	if( (res = SPIReadSaveData(type, offset1, &buf2, 1)) ) return res;
	buf3=~buf1;
	if( (res = SPIWriteSaveData(type, offset0, &buf3, 1)) ) return res;
	if( (res = SPIReadSaveData(type, offset1, &buf4, 1)) ) return res;
	if( (res = SPIWriteSaveData(type, offset0, &buf1, 1)) ) return res;
	
	*mirrored = buf2 != buf4;
	return 0;
}

int SPIGetCardType(CardType* type, int infrared) {
	u8 sr = 0;
	u32 jedec = 0;
	u32 tries = 0;
	CardType t = (infrared == 1) ? &FLASH_INFRARED_DUMMY : &FLASH_STD_DUMMY;
	int res; 
	
	u32 maxTries = (infrared == -1) ? 2 : 1; // note: infrared = -1 fails 1/3 of the time
	while(tries < maxTries){ 
		res = SPIReadJEDECIDAndStatusReg(t, &jedec, &sr); // dummy
		if(res) return res;
		
		if ((sr & 0xfd) == 0x00 && (jedec != 0x00ffffff)) { break; }		
		if ((sr & 0xfd) == 0xF0 && (jedec == 0x00ffffff)) { t = EEPROM_512B; break; }
		if ((sr & 0xfd) == 0x00 && (jedec == 0x00ffffff)) { t = &EEPROM_STD_DUMMY; break; }
		
		++tries;
		t = &FLASH_INFRARED_DUMMY;
	}
	
	if(t == EEPROM_512B) { *type = t; return 0; }
	else if(t == &EEPROM_STD_DUMMY) {
		bool mirrored = false;
		size_t i;
		
		for(i = 0; i < sizeof(EEPROMTypes) / sizeof(CardTypeData) - 1; i++) {
			if( (res = _SPIIsDataMirrored(t, SPIGetCapacity(EEPROMTypes + i), &mirrored)) ) return res;
			if(mirrored) {
				*type = EEPROMTypes + i;
				return 0;
			}
		}
		*type = EEPROMTypes + i;
		return 0;
	}
	
	else if(t == &FLASH_INFRARED_DUMMY) {
		size_t i;
		
		if(infrared == 0) *type = NO_CHIP; // did anything go wrong?
		
		for(i = 0; i < sizeof(FlashInfraredTypes) / sizeof(CardTypeData); i++) {
			if(FlashInfraredTypes[i].jedecId == jedec) {
				*type = FlashInfraredTypes + i;
				return 0;
			}
		}
		
		*type = NO_CHIP;
		return 0;
	}
	
	else {
		size_t i;
		
		if(infrared == 1) *type = NO_CHIP; // did anything go wrong?
		
		for(i = 0; i < sizeof(FlashStdTypes) / sizeof(CardTypeData); i++) {
			if(FlashStdTypes[i].jedecId == jedec) {
				*type = FlashStdTypes + i;
				return 0;
			}
		}
		
		*type = NO_CHIP;
		return 0;
	}
}
