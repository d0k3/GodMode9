#pragma once

#include "common.h"
#include "virtual.h"

bool ReadVMemDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVMemFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
int WriteVMemFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count);
