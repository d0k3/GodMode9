#pragma once

#include "virtual.h"

void DeinitVSaveDrive(void);
u64 InitVSaveDrive(void);
u64 CheckVSaveDrive(void);

bool ReadVSaveDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVSaveFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
bool VSaveIsExtData(void);

u64 GetVSaveDriveSize(void);