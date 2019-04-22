#pragma once

#include <types.h>

#include "hw/spi.h"

#define NVRAM_SR_WIP	BIT(0) // work in progress / busy
#define NVRAM_SR_WEL	BIT(1) // write enable latch

u32 NVRAM_Status(void);
u32 NVRAM_ReadID(void);

void NVRAM_Read(u32 offset, u32 *buffer, u32 len);

void NVRAM_DeepStandby(void);
void NVRAM_Wakeup(void);
