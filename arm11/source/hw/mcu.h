/*
 *   This file is part of GodMode9
 *   Copyright (C) 2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <types.h>

#include "arm/timer.h"
#include "hw/i2c.h"

#define MCU_INTERRUPT	(0x71)
#define I2C_MCU_DEVICE	(3)

u8 MCU_GetVolumeSlider(void);
u32 MCU_GetSpecialHID(void);

void MCU_SetNotificationLED(u32 period_ms, u32 color);
void MCU_ResetLED(void);

void MCU_HandleInterrupts(u32 irqn);

void MCU_Init(void);

static inline u8 MCU_ReadReg(u8 addr)
{
	u8 val;
	I2C_readRegBuf(I2C_MCU_DEVICE, addr, &val, 1);
	return val;
}

static inline bool MCU_ReadRegBuf(u8 addr, u8 *buf, u32 size)
{
	return I2C_readRegBuf(I2C_MCU_DEVICE, addr, buf, size);
}

static inline bool MCU_WriteReg(u8 addr, u8 val)
{
	return I2C_writeRegBuf(I2C_MCU_DEVICE, addr, &val, 1);
}

static inline bool MCU_WriteRegBuf(u8 addr, const u8 *buf, u32 size)
{
	return I2C_writeRegBuf(I2C_MCU_DEVICE, addr, buf, size);
}

static inline u32 MCU_waitEvents(u32 mask) {
	u32 v;
	while(1) {
		TIMER_WaitMS(10);
		MCU_ReadRegBuf(0x10, (u8*)&v, 4);
		v &= mask;
		if (v)
			break;
	}
	return v;
}

static inline void MCU_controlLCDPower(u8 bits)
{
	MCU_WriteReg(0x22u, bits);
}
