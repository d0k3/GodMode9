#pragma once

#include "common.h"

#define BRIGHTNESS_AUTOMATIC (-1)
#define BRIGHTNESS_MIN (10)
#define BRIGHTNESS_MAX (210)

u32 SetScreenBrightness(int level);
u32 GetBatteryPercent();
bool IsCharging();
void Reboot();
void PowerOff();
