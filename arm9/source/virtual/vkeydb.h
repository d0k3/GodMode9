#pragma once

#include "common.h"
#include "virtual.h"

u32 InitVKeyDbDrive(void);
u32 CheckVKeyDbDrive(void);

bool ReadVKeyDbDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVKeyDbFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
// int WriteVKeyDbFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count); // no writing
u64 GetVKeyDbDriveSize(void);
