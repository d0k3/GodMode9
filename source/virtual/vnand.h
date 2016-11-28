#pragma once

#include "common.h"
#include "virtual.h"

bool CheckVNandDrive(u32 nand_src);
bool ReadVNandDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVNandFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
int WriteVNandFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count);
