#pragma once

#include "common.h"
#include "virtual.h"

bool FindVMemFile(VirtualFile* vfile, const char* name, u32 size);
int ReadVMemFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
int WriteVMemFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count);
