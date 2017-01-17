#pragma once

#include "common.h"
#include "virtual.h"

u32 InitVCartDrive(void);
bool ReadVCartDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVCartFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
// int WriteVCartFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count); // no writes
u64 GetVCartDriveSize(void);
