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

#include <stdatomic.h>

#include "arm/timer.h"

#include "hw/gpio.h"
#include "hw/gpulcd.h"
#include "hw/mcu.h"

#include "system/event.h"

#define MCUEV_HID_MASK ( \
	MCUEV_HID_PWR_DOWN | MCUEV_HID_PWR_HOLD | \
	MCUEV_HID_HOME_DOWN | MCUEV_HID_HOME_UP | MCUEV_HID_WIFI_SWITCH)

enum {
	MCUREG_VOLUME_SLIDER = 0x09,

	MCUREG_BATTERY_LEVEL = 0x0B,
	MCUREG_CONSOLE_STATE = 0x0F,

	MCUREG_INT_MASK = 0x10,
	MCUREG_INT_EN = 0x18,

	MCUREG_LCD_STATE = 0x22,

	MCUREG_LED_WIFI = 0x2A,
	MCUREG_LED_CAMERA = 0x2B,
	MCUREG_LED_SLIDER = 0x2C,
	MCUREG_LED_STATUS = 0x2D,

	MCUREG_RTC = 0x30,
};

typedef struct {
	u8 delay;
	u8 smoothing;
	u8 loop_delay;
	u8 unk;
	u32 red[8];
	u32 green[8];
	u32 blue[8];
} PACKED_STRUCT mcuStatusLED;

static u8 volumeSliderValue;
static u32 shellState;
static _Atomic(u32) pendingEvents;

static void mcuEventUpdate(void)
{
	u32 mask;

	// lazily update the mask on each test attempt
	if (!getEventIRQ()->test(MCU_INTERRUPT, true))
		return;

	// reading the pending mask automagically acknowledges
	// the interrupts so all of them must be processed in one go
	mcuReadRegBuf(MCUREG_INT_MASK, (u8*)&mask, sizeof(mask));

	if (mask & MCUEV_HID_VOLUME_SLIDER)
		volumeSliderValue = mcuReadReg(MCUREG_VOLUME_SLIDER);

	if (mask & MCUEV_HID_SHELL_OPEN) {
		mcuResetLEDs();
		shellState = SHELL_OPEN;
	}

	if (mask & MCUEV_HID_SHELL_CLOSE) {
		shellState = SHELL_CLOSED;
	}

	atomic_fetch_or(&pendingEvents, mask);
}

u8 mcuGetVolumeSlider(void)
{
	mcuEventUpdate();
	return volumeSliderValue;
}

u32 mcuGetSpecialHID(void)
{
	u32 ret = 0, pend = getEventMCU()->test(MCUEV_HID_MASK, MCUEV_HID_MASK);

	// hopefully gets unrolled
	if (pend & (MCUEV_HID_PWR_DOWN | MCUEV_HID_PWR_HOLD))
		ret |= BUTTON_POWER;
	if (pend & MCUEV_HID_HOME_DOWN)
		ret |= BUTTON_HOME;
	if (pend & MCUEV_HID_HOME_UP)
		ret &= ~BUTTON_HOME;
	return ret | shellState;
}

void mcuSetStatusLED(u32 period_ms, u32 color)
{
	u32 r, g, b;
	mcuStatusLED ledState;

	// handle proper non-zero periods
	// so small the hardware can't handle it
	if (period_ms != 0 && period_ms < 63)
		period_ms = 63;

	ledState.delay = (period_ms * 0x10) / 1000;
	ledState.smoothing = 0x40;
	ledState.loop_delay = 0x10;
	ledState.unk = 0;

	// all colors look like 0x00ZZ00ZZ
	// in order to alternate between
	// LED "off" and the wanted color
	r = (color >> 16) & 0xFF;
	r |= r << 16;
	for (int i = 0; i < 8; i++)
		ledState.red[i] = r;

	g = (color >> 8) & 0xFF;
	g |= g << 16;
	for (int i = 0; i < 8; i++)
		ledState.green[i] = g;

	b = color & 0xFF;
	b |= b << 16;
	for (int i = 0; i < 8; i++)
		ledState.blue[i] = b;

	mcuWriteRegBuf(MCUREG_LED_STATUS, (const u8*)&ledState, sizeof(ledState));
}

void mcuResetLEDs(void)
{
	mcuWriteReg(MCUREG_LED_WIFI, 0);
	mcuWriteReg(MCUREG_LED_CAMERA, 0);
	mcuWriteReg(MCUREG_LED_SLIDER, 0);
	mcuSetStatusLED(0, 0);
}

void mcuReset(void)
{
	u32 intmask = 0;

	atomic_init(&pendingEvents, 0);

	// set register mask and clear any pending registers
	mcuWriteRegBuf(MCUREG_INT_EN, (const u8*)&intmask, sizeof(intmask));
	mcuReadRegBuf(MCUREG_INT_MASK, (u8*)&intmask, sizeof(intmask));

	mcuResetLEDs();

	volumeSliderValue = mcuReadReg(MCUREG_VOLUME_SLIDER);
	shellState = SHELL_OPEN;
	// assume the shell is always open on boot
	// knowing the average 3DS user, there will be plenty
	// of laughs when this comes back to bite us in the rear

	GPIO_setBit(19, 9);
}

static void evReset(void) {
	atomic_store(&pendingEvents, 0);
}

static u32 evTest(u32 mask, u32 clear) {
	mcuEventUpdate();
	return atomic_fetch_and(&pendingEvents, ~clear) & mask;
}

static const EventInterface evMCU = {
	.reset = evReset,
	.test = evTest
};

const EventInterface *getEventMCU(void) {
	return &evMCU;
}

