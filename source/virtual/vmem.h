#pragma once

#include "common.h"
#include "virtual.h"

bool ReadVMemDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVMemFile(const VirtualFile* vfile, u8* buffer, u64 offset, u64 count);
int WriteVMemFile(const VirtualFile* vfile, const u8* buffer, u64 offset, u64 count);
