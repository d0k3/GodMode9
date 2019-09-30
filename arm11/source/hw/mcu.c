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

#include <hid_map.h>
#include <types.h>
#include <arm.h>

#include "arm/timer.h"

#include "hw/gpio.h"
#include "hw/gpulcd.h"
#include "hw/mcu.h"

enum {
	MCU_PWR_BTN = 0,
	MCU_PWR_HOLD = 1,
	MCU_HOME_BTN = 2,
	MCU_HOME_LIFT = 3,
	MCU_WIFI_SWITCH = 4,
	MCU_SHELL_CLOSE = 5,
	MCU_SHELL_OPEN = 6,
	MCU_VOL_SLIDER = 22,
};

enum {
	REG_VOL_SLIDER = 0x09,

	REG_BATTERY_LEVEL = 0x0B,
	REG_CONSOLE_STATE = 0x0F,

	REG_INT_MASK = 0x10,
	REG_INT_EN = 0x18,

	REG_LCD_STATE = 0x22,

	REG_LED_WIFI = 0x2A,
	REG_LED_CAMERA = 0x2B,
	REG_LED_SLIDER = 0x2C,
	REG_LED_NOTIF = 0x2D,

	REG_RTC = 0x30,
};

typedef struct {
	u8 delay;
	u8 smoothing;
	u8 loop_delay;
	u8 unk;
	u32 red[8];
	u32 green[8];
	u32 blue[8];
} PACKED_STRUCT MCU_NotificationLED;

static u8 cached_volume_slider = 0;
static u32 spec_hid = 0, shell_state = SHELL_OPEN;

static void MCU_UpdateVolumeSlider(void)
{
	cached_volume_slider = MCU_ReadReg(REG_VOL_SLIDER);
}

static void MCU_UpdateShellState(bool open)
{
	shell_state = open ? SHELL_OPEN : SHELL_CLOSED;
}

u8 MCU_GetVolumeSlider(void)
{
	return cached_volume_slider;
}

u32 MCU_GetSpecialHID(void)
{
	u32 ret = spec_hid | shell_state;
	spec_hid = 0;
	return ret;
}

void MCU_SetNotificationLED(u32 period_ms, u32 color)
{
	u32 r, g, b;
	MCU_NotificationLED led_state;

	// handle proper non-zero periods
	// so small the hardware can't handle it
	if (period_ms != 0 && period_ms < 63)
		period_ms = 63;

	led_state.delay = (period_ms * 0x10) / 1000;
	led_state.smoothing = 0x40;
	led_state.loop_delay = 0x10;
	led_state.unk = 0;

	// all colors look like 0x00ZZ00ZZ
	// in order to alternate between
	// LED "off" and the wanted color
	r = (color >> 16) & 0xFF;
	r |= r << 16;
	for (int i = 0; i < 8; i++)
		led_state.red[i] = r;

	g = (color >> 8) & 0xFF;
	g |= g << 16;
	for (int i = 0; i < 8; i++)
		led_state.green[i] = g;

	b = color & 0xFF;
	b |= b << 16;
	for (int i = 0; i < 8; i++)
		led_state.blue[i] = b;

	MCU_WriteRegBuf(REG_LED_NOTIF, (const u8*)&led_state, sizeof(led_state));
}

void MCU_ResetLED(void)
{
	MCU_WriteReg(REG_LED_WIFI, 0);
	MCU_WriteReg(REG_LED_CAMERA, 0);
	MCU_WriteReg(REG_LED_SLIDER, 0);
	MCU_SetNotificationLED(0, 0);
}

void MCU_PushToLCD(bool enable)
{
	MCU_WriteReg(REG_LCD_STATE, enable ? 0x2A : 0x01);
}

void MCU_HandleInterrupts(u32 __attribute__((unused)) irqn)
{
	u32 ints;

	// Reading the pending mask automagically acknowledges
	// the interrupts so all of them must be processed in one go
	MCU_ReadRegBuf(REG_INT_MASK, (u8*)&ints, sizeof(ints));

	while(ints != 0) {
		u32 mcu_int_id = 31 - __builtin_clz(ints);

		switch(mcu_int_id) {
			case MCU_PWR_BTN:
			case MCU_PWR_HOLD:
				spec_hid |= BUTTON_POWER;
				break;

			case MCU_HOME_BTN:
				spec_hid |= BUTTON_HOME;
				break;

			case MCU_HOME_LIFT:
				spec_hid &= ~BUTTON_HOME;
				break;

			case MCU_WIFI_SWITCH:
				spec_hid |= BUTTON_WIFI;
				break;

			case MCU_SHELL_OPEN:
				MCU_PushToLCD(true);
				MCU_UpdateShellState(true);
				TIMER_WaitTicks(CLK_MS_TO_TICKS(5));
				MCU_ResetLED();
				break;

			case MCU_SHELL_CLOSE:
				MCU_PushToLCD(false);
				MCU_UpdateShellState(false);
				TIMER_WaitTicks(CLK_MS_TO_TICKS(5));
				break;

			case MCU_VOL_SLIDER:
				MCU_UpdateVolumeSlider();
				break;

			default:
				break;
		}

		ints &= ~BIT(mcu_int_id);
	}
}

void MCU_Init(void)
{
	u32 clrpend, mask = 0xFFBF0800;

	/* set register mask and clear any pending registers */
	MCU_WriteRegBuf(REG_INT_EN, (const u8*)&mask, sizeof(mask));
	MCU_ReadRegBuf(REG_INT_MASK, (u8*)&clrpend, sizeof(clrpend));

	MCU_ResetLED();

	MCU_UpdateVolumeSlider();
	MCU_UpdateShellState(MCU_ReadReg(REG_CONSOLE_STATE) & BIT(1));

	GPIO_setBit(19, 9);
}
