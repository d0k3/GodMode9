#pragma once

#include "common.h"

int ReadRamDriveSectors(u8* buffer, u32 sector, u32 count);
int WriteRamDriveSectors(const u8* buffer, u32 sector, u32 count);
u64 GetRamDriveSize(void);
void InitRamDrive(void);
