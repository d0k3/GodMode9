// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common.h"

void CTR_CmdReadSectorSD(u8* aBuffer, u32 aSector);
void CTR_CmdReadData(u32 sector, u32 length, u32 blocks, void* buffer);
void CTR_CmdReadHeader(void* buffer);
void CTR_CmdReadUniqueID(void* buffer);
u32 CTR_CmdGetSecureId(u32 rand1, u32 rand2);
void CTR_CmdSeed(u32 rand1, u32 rand2);
