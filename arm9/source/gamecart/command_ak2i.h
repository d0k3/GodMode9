// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// modifyed by osilloscopion (2 Jul 2016)
//

#pragma once

#include "common.h"

enum {
    AK2I_MTN_NOR_OFFSET      = 0,
};

u32 AK2I_CmdGetHardwareVersion(void);
void AK2I_CmdReadRom(u32 address, u8 *buffer, u32 length);
void AK2I_CmdReadFlash(u32 address, u8 *buffer, u32 length);
void AK2I_CmdSetMapTableAddress(u32 tableName, u32 tableInRamAddress);
void AK2I_CmdSetFlash1681_81(void);
void AK2I_CmdUnlockFlash(void);
void AK2I_CmdUnlockASIC(void);
void AK2i_CmdLockFlash(void);
void AK2I_CmdActiveFatMap(void);
void AK2I_CmdEraseFlashBlock_44(u32 address);
void AK2I_CmdEraseFlashBlock_81(u32 address);
void AK2I_CmdWriteFlash_44(u32 address, const void *data, u32 length);
void AK2I_CmdWriteFlash_81(u32 address, const void *data, u32 length);
bool AK2I_CmdVerifyFlash(void *src, u32 dest, u32 length);
