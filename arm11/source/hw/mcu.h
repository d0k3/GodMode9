#pragma once

#include <types.h>

#include "hw/i2c.h"

#define MCU_INTERRUPT	(0x71)
#define I2C_MCU_DEVICE	(3)

u8 MCU_GetVolumeSlider(void);
u32 MCU_GetSpecialHID(void);

void MCU_SetNotificationLED(u32 period_ms, u32 color);
void MCU_ResetLED(void);

void MCU_PushToLCD(bool enable);

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
