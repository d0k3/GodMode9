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

enum {
	MCUEV_HID_PWR_DOWN = BIT(0),
	MCUEV_HID_PWR_HOLD = BIT(1),
	MCUEV_HID_HOME_DOWN = BIT(2),
	MCUEV_HID_HOME_UP = BIT(3),
	MCUEV_HID_WIFI_SWITCH = BIT(4),
	MCUEV_HID_SHELL_CLOSE = BIT(5),
	MCUEV_HID_SHELL_OPEN = BIT(6),
	MCUEV_HID_VOLUME_SLIDER = BIT(22),
};

u8 mcuGetVolumeSlider(void);
u32 mcuGetSpecialHID(void);

void mcuSetStatusLED(u32 period_ms, u32 color);
void mcuResetLEDs(void);

void mcuReset(void);

static inline u8 mcuReadReg(u8 addr)
{
	u8 val;
	I2C_readRegBuf(I2C_MCU_DEVICE, addr, &val, 1);
	return val;
}

static inline bool mcuReadRegBuf(u8 addr, u8 *buf, u32 size)
{
	return I2C_readRegBuf(I2C_MCU_DEVICE, addr, buf, size);
}

static inline bool mcuWriteReg(u8 addr, u8 val)
{
	return I2C_writeRegBuf(I2C_MCU_DEVICE, addr, &val, 1);
}

static inline bool mcuWriteRegBuf(u8 addr, const u8 *buf, u32 size)
{
	return I2C_writeRegBuf(I2C_MCU_DEVICE, addr, buf, size);
}

static inline void MCU_controlLCDPower(u8 bits)
{
	mcuWriteReg(0x22u, bits);
}
