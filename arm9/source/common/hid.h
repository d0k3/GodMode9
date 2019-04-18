#pragma once

#include "common.h"

#include "hid_map.h"

// see: http://3dbrew.org/wiki/CONFIG9_Registers
// see: http://3dbrew.org/wiki/EMMC_Registers

u32 HID_ReadState(void);
u32 HID_ReadRawTouchState(void);

#define CART_STATE (~(*(volatile u8*)0x10000010) & 0x1)
#define SD_STATE ((*(volatile u16*)0x1000601C) & (0x1<<5))

u32 InputWait(u32 timeout_sec);
bool CheckButton(u32 button);

void ButtonToString(u32 button, char* str);
u32 StringToButton(char* str);
