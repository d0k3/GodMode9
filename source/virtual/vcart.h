#pragma once

#include "common.h"
#include "virtual.h"

u32 InitVCartDrive(void);
bool ReadVCartDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVCartFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
// int WriteVCartFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count); // no writes
u64 GetVCartDriveSize(void);
