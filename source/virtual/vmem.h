#pragma once

#include "common.h"
#include "virtual.h"

bool ReadVMemDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVMemFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
int WriteVMemFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count);
