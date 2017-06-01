#pragma once

#include "common.h"

int ReadRamDriveSectors(void* buffer, u32 sector, u32 count);
int WriteRamDriveSectors(const void* buffer, u32 sector, u32 count);
u64 GetRamDriveSize(void);
void InitRamDrive(void);
