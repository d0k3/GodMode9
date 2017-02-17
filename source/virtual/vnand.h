#pragma once

#include "common.h"
#include "virtual.h"

bool CheckVNandDrive(u32 nand_src);
bool ReadVNandDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVNandFile(const VirtualFile* vfile, u8* buffer, u64 offset, u64 count);
int WriteVNandFile(const VirtualFile* vfile, const u8* buffer, u64 offset, u64 count);
u64 GetVNandDriveSize(u32 nand_src);
