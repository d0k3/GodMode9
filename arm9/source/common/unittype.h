#pragma once

#include "common.h"

// see: https://3dbrew.org/wiki/CONFIG11_Registers
#define IS_O3DS     (((*(vu16*) 0x10140FFC) & 2) == 0)

// see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
// see: https://www.3dbrew.org/wiki/OTP_Registers#Plaintext_OTP
#define IS_DEVKIT   ((*(vu8*) (0x01FFB800+0x19)) != 0x0)

// https://www.3dbrew.org/wiki/CONFIG9_Registers
// (actually checks for an unlocked OTP, meaning sighax)
#define IS_UNLOCKED (!((*(vu8*)0x10000000) & 0x2))

// System models
enum SystemModel {
    MODEL_OLD_3DS = 0,
    MODEL_OLD_3DS_XL,
    MODEL_NEW_3DS,
    MODEL_OLD_2DS,
    MODEL_NEW_3DS_XL,
    MODEL_NEW_2DS_XL,
    NUM_MODELS
};
