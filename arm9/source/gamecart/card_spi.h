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

typedef struct CardSPITypeData CardSPITypeData;

typedef struct {
    const CardSPITypeData *chip;
    bool infrared;
} CardSPIType;

struct CardSPITypeData {
    int (*enableWriting) (CardSPIType type);
    int (*readSaveData) (CardSPIType type, u32 offset, void* data, u32 size);
    int (*writeSaveData) (CardSPIType type, u32 offset, const void* data, u32 size);
    int (*eraseSector) (CardSPIType type, u32 offset);
    u32 jedecId;
    u32 capacity;
    u32 eraseSize;
    u32 pageSize;
    u32 writeSize;
    u8 writeCommand;
    u8 programCommand;
    u8 eraseCommand;
};

#define NO_CHIP NULL

const CardSPITypeData * const EEPROM_512B;

const CardSPITypeData * const EEPROM_8KB;
const CardSPITypeData * const EEPROM_64KB;
const CardSPITypeData * const EEPROM_128KB;

const CardSPITypeData * const FLASH_256KB_1;
const CardSPITypeData * const FLASH_256KB_2;
const CardSPITypeData * const FLASH_512KB_1;
const CardSPITypeData * const FLASH_512KB_2;
const CardSPITypeData * const FLASH_1MB;
const CardSPITypeData * const FLASH_8MB;

const CardSPITypeData * const FLASH_128KB_CTR; // Most common, including Ocarina of time 3D
const CardSPITypeData * const FLASH_512KB_CTR; // Also common, including Detective Pikachu
const CardSPITypeData * const FLASH_1MB_CTR; // For example Pokemon Ultra Sun

int CardSPIWriteRead(CardSPIType type, const void* cmd, u32 cmdSize, void* answer, u32 answerSize, const void* data, u32 dataSize);
int CardSPIWaitWriteEnd(CardSPIType type, u32 timeout);
int CardSPIEnableWriting(CardSPIType type);
int CardSPIReadJEDECIDAndStatusReg(CardSPIType type, u32* id, u8* statusReg);
int CardSPIGetCardSPIType(CardSPIType* type, bool infrared);
u32 CardSPIGetPageSize(CardSPIType type);
u32 CardSPIGetCapacity(CardSPIType type);
u32 CardSPIGetEraseSize(CardSPIType type);

int CardSPIWriteSaveData(CardSPIType type, u32 offset, const void* data, u32 size);
int CardSPIReadSaveData(CardSPIType type, u32 offset, void* data, u32 size);

int CardSPIEraseSector(CardSPIType type, u32 offset);
int CardSPIErase(CardSPIType type);

#ifdef __cplusplus
}
#endif
