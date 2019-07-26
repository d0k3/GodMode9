/*
 *  This file is based on SPI.h from TWLSaveTool. Its copyright notice is
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

#pragma once
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_CMD_RDSR 5
#define SPI_CMD_WREN 6

#define SPI_512B_EEPROM_CMD_WRLO 2
#define SPI_512B_EEPROM_CMD_WRHI 10
#define SPI_512B_EEPROM_CMD_RDLO 3
#define SPI_512B_EEPROM_CMD_RDHI 11

#define SPI_EEPROM_CMD_WRITE 2 

#define SPI_CMD_READ 3

#define SPI_CMD_PP 2
#define SPI_FLASH_CMD_PW 10
#define SPI_FLASH_CMD_RDID 0x9f
#define SPI_FLASH_CMD_SE 0xd8
#define SPI_FLASH_CMD_PE 0xdb
#define SPI_FLASH_CMD_MXIC_SE 0x20

#define SPI_FLG_WIP 1
#define SPI_FLG_WEL 2

typedef struct CardTypeData CardTypeData;

typedef const CardTypeData * CardType;

struct CardTypeData {
	int (*enableWriting) (CardType type);
	int (*readSaveData) (CardType type, u32 offset, void* data, u32 size);
	int (*writeSaveData) (CardType type, u32 offset, const void* data, u32 size);
	int (*eraseSector) (CardType type, u32 offset);
	u32 jedecId;
	u32 capacity;
	u32 eraseSize;
	u32 pageSize;
	u32 writeSize;
	bool infrared;
	u8 writeCommand;
	u8 programCommand;
	u8 eraseCommand;
};

#define NO_CHIP NULL

const CardType EEPROM_512B;

const CardType EEPROM_8KB;
const CardType EEPROM_64KB;
const CardType EEPROM_128KB;

const CardType FLASH_256KB_1;
const CardType FLASH_256KB_2;
const CardType FLASH_512KB_1;
const CardType FLASH_512KB_2;
const CardType FLASH_1MB;
const CardType FLASH_8MB_1; // <- can't restore savegames, and maybe not read them atm
const CardType FLASH_8MB_2; // we are also unsure about the ID for this

const CardType FLASH_64KB_CTR; // I am extrapolating from the dataheets, only a few of these have been observed in the wild
const CardType FLASH_128KB_CTR; // Most common, including Ocarina of time 3D
const CardType FLASH_256KB_CTR;
const CardType FLASH_512KB_CTR; // Also common, including Detective Pikachu
const CardType FLASH_1MB_CTR; // For example Pokemon Ultra Sun
const CardType FLASH_2MB_CTR;
const CardType FLASH_4MB_CTR;
const CardType FLASH_8MB_CTR;

const CardType FLASH_256KB_1_INFRARED; // AFAIK, only "Active Health with Carol Vorderman" has such a flash save memory
const CardType FLASH_256KB_2_INFRARED;
const CardType FLASH_512KB_1_INFRARED;
const CardType FLASH_512KB_2_INFRARED;

int SPIWriteRead(CardType type, void* cmd, u32 cmdSize, void* answer, u32 answerSize, const void* data, u32 dataSize);
int SPIWaitWriteEnd(CardType type);
int SPIEnableWriting(CardType type);
int SPIReadJEDECIDAndStatusReg(CardType type, u32* id, u8* statusReg);
int SPIGetCardType(CardType* type, int infrared);
u32 SPIGetPageSize(CardType type);
u32 SPIGetCapacity(CardType type);
u32 SPIGetEraseSize(CardType type);

int SPIWriteSaveData(CardType type, u32 offset, const void* data, u32 size);
int SPIReadSaveData(CardType type, u32 offset, void* data, u32 size);

int SPIEraseSector(CardType type, u32 offset);
int SPIErase(CardType type);

#ifdef __cplusplus
}
#endif
