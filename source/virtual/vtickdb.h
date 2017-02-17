#pragma once

#include "common.h"
#include "virtual.h"

u32 InitVTickDbDrive(void);
u32 CheckVTickDbDrive(void);

bool ReadVTickDbDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVTickDbFile(const VirtualFile* vfile, u8* buffer, u64 offset, u64 count);
// int WriteVTickDbFile(const VirtualFile* vfile, const u8* buffer, u64 offset, u64 count); // no writing
u64 GetVTickDbDriveSize(void);
