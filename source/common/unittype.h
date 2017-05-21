#pragma once

#include "common.h"

// see: https://3dbrew.org/wiki/CONFIG11_Registers
#define IS_O3DS     ((*(vu32*) 0x10140FFC) != 0x7)

// see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
// see: https://www.3dbrew.org/wiki/OTP_Registers#Plaintext_OTP
#define IS_DEVKIT   ((*(vu8*) (0x01FFB800+0x19)) != 0x0)

// see: https://3dbrew.org/wiki/CONFIG11_Registers
// (also returns true for sighaxed systems, maybe change the name later?)
#define IS_A9LH     ((*(vu32*) 0x101401C0) == 0)

// https://www.3dbrew.org/wiki/CONFIG9_Registers
// (actually checks for an unlocked OTP)
#define IS_UNLOCKED (!((*(vu8*)0x10000000) & 0x2))

// A9LH + unlocked == SigHax
#define IS_SIGHAX   (IS_A9LH && IS_UNLOCKED)
