#pragma once

#include "common.h"

#define HID_STATE (~(*(volatile u32*)0x10146000) & BUTTON_ANY)


#define BUTTON_A      (1 << 0)
#define BUTTON_B      (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START  (1 << 3)
#define BUTTON_RIGHT  (1 << 4)
#define BUTTON_LEFT   (1 << 5)
#define BUTTON_UP     (1 << 6)
#define BUTTON_DOWN   (1 << 7)
#define BUTTON_R1     (1 << 8)
#define BUTTON_L1     (1 << 9)
#define BUTTON_X      (1 << 10)
#define BUTTON_Y      (1 << 11)
#define BUTTON_ANY    0x00000FFF
#define BUTTON_ARROW  (BUTTON_RIGHT|BUTTON_LEFT|BUTTON_UP|BUTTON_DOWN)

// special buttons
#define BUTTON_POWER  (1 << 12)
#define BUTTON_HOME   (1 << 13)

u32 InputWait();
bool CheckButton(u32 button);
