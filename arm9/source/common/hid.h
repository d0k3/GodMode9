#pragma once

#include "common.h"

// see: http://3dbrew.org/wiki/CONFIG9_Registers
// see: http://3dbrew.org/wiki/EMMC_Registers
#define HID_STATE (~(*(volatile u32*)0x10146000) & BUTTON_ANY)
#define CART_STATE (~(*(volatile u8*)0x10000010) & 0x1)
#define SD_STATE ((*(volatile u16*)0x1000601C) & (0x1<<5))


#define BUTTON_A      ((u32)1 << 0)
#define BUTTON_B      ((u32)1 << 1)
#define BUTTON_SELECT ((u32)1 << 2)
#define BUTTON_START  ((u32)1 << 3)
#define BUTTON_RIGHT  ((u32)1 << 4)
#define BUTTON_LEFT   ((u32)1 << 5)
#define BUTTON_UP     ((u32)1 << 6)
#define BUTTON_DOWN   ((u32)1 << 7)
#define BUTTON_R1     ((u32)1 << 8)
#define BUTTON_L1     ((u32)1 << 9)
#define BUTTON_X      ((u32)1 << 10)
#define BUTTON_Y      ((u32)1 << 11)
#define BUTTON_ANY    0x00000FFF
#define BUTTON_ARROW  (BUTTON_RIGHT|BUTTON_LEFT|BUTTON_UP|BUTTON_DOWN)

// special buttons / cart / sd
#define BUTTON_POWER  ((u32)1 << 12)
#define BUTTON_HOME   ((u32)1 << 13)
#define CART_INSERT   ((u32)1 << 14)
#define CART_EJECT    ((u32)1 << 15)
#define SD_INSERT     ((u32)1 << 16)
#define SD_EJECT      ((u32)1 << 17)
#define TIMEOUT_HID   ((u32)1 << 31)

// strings for button conversion
#define BUTTON_STRINGS  "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L", "X", "Y"


u32 InputWait(u32 timeout_sec);
bool CheckButton(u32 button);

void ButtonToString(u32 button, char* str);
u32 StringToButton(char* str);
