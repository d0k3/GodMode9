/*
 *  This file is part of TWLSaveTool.
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


#define SPI_FLG_WIP 1
#define SPI_FLG_WEL 2

// extern u8* fill_buf; 
typedef enum {
	NO_CHIP = -1,
	
	EEPROM_512B = 0,
	
	EEPROM_8KB = 1,
	EEPROM_64KB = 2,
	EEPROM_128KB = 3,
	EEPROM_STD_DUMMY = 1,
	
	FLASH_256KB_1 = 4,
	FLASH_256KB_2 = 5,
	FLASH_512KB_1 = 6,
	FLASH_512KB_2 = 7,
	FLASH_1MB = 8,
	FLASH_8MB = 9, // <- can't restore savegames, and maybe not read them atm
	FLASH_STD_DUMMY = 4,
	
	FLASH_64KB_CTR = 10, // I am extrapolating from the dataheets, only a few of these have been observed in the wild
	FLASH_128KB_CTR = 11, // Most common, including Ocarina of time 3D
	FLASH_256KB_CTR = 12,
	FLASH_512KB_CTR = 13, // Also common, including Detective Pikachu
	FLASH_1MB_CTR = 14, // For example Pokemon Ultra Sun
	FLASH_2MB_CTR = 15,
	FLASH_4MB_CTR = 16,
	FLASH_8MB_CTR = 17,
	// Animal crossing: New leaf???
	// (What is that? 3dbrew states 10M, but Macronix only makes powers of 2)

	FLASH_512KB_INFRARED = 18,
	FLASH_256KB_INFRARED = 19, // AFAIK, only "Active Health with Carol Vorderman" has such a flash save memory
	FLASH_INFRARED_DUMMY = 17,
	
	CHIP_LAST = 19,
} CardType;

int SPIWriteRead(CardType type, void* cmd, u32 cmdSize, void* answer, u32 answerSize, void* data, u32 dataSize);
int SPIWaitWriteEnd(CardType type);
int SPIEnableWriting(CardType type);
int SPIReadJEDECIDAndStatusReg(CardType type, u32* id, u8* statusReg);
int SPIGetCardType(CardType* type, int infrared);
u32 SPIGetPageSize(CardType type);
u32 SPIGetCapacity(CardType type);

int SPIWriteSaveData(CardType type, u32 offset, void* data, u32 size);
int SPIReadSaveData(CardType type, u32 offset, void* data, u32 size);

// int SPIEraseSector(CardType type, u32 offset);
// int SPIErase(CardType type); 

#ifdef __cplusplus
}
#endif
