// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// modifyed by osilloscopion (2 Jul 2016)
//

#pragma once

#include "common.h"

void NTR_CmdReset(void);
u32 NTR_CmdGetCartId(void);
void NTR_CmdEnter16ByteMode(void);
void NTR_CmdReadHeader (u8* buffer);
void NTR_CmdReadData (u32 offset, void* buffer);

bool NTR_Secure_Init (u8* buffer, u32 CartID, int iCardDevice);

