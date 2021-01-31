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
    u16 jedecId;
    u32 capacity;
    u32 eraseSize;
    u32 pageSize;
    u32 writeSize;
    u8 writeCommand;
    u8 programCommand;
    u8 eraseCommand;
};

#define NO_CHIP NULL

extern const CardSPITypeData * const EEPROM_512B;

extern const CardSPITypeData * const EEPROM_8KB;
extern const CardSPITypeData * const EEPROM_64KB;
extern const CardSPITypeData * const EEPROM_128KB;

extern const CardSPITypeData * const FLASH_NTR_GENERIC; // Most common flash chip in DS games, in 3 different sizes
extern const CardSPITypeData * const FLASH_256KB;
extern const CardSPITypeData * const FLASH_512KB;
extern const CardSPITypeData * const FLASH_8MB;

extern const CardSPITypeData * const FLASH_CTR_GENERIC; // Handles each 3ds cartridge the exact same

int CardSPIWriteRead(bool infrared, const void* cmd, u32 cmdSize, void* answer, u32 answerSize, const void* data, u32 dataSize);
int CardSPIWaitWriteEnd(bool infrared, u32 timeout);
int CardSPIEnableWriting(CardSPIType type);
int CardSPIReadJEDECIDAndStatusReg(bool infrared, u32* id, u8* statusReg);
CardSPIType CardSPIGetCardSPIType(bool infrared);
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
