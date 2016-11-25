#pragma once

#include "common.h"
#include "filetype.h"
#include "virtual.h"

u32 MountVGameFile(const char* path);
u32 CheckVGameDrive(void);

bool ReadVGameDir(VirtualFile* vfile, const char* path);
int ReadVGameFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
// int WriteVGameFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count); // writing is not enabled
