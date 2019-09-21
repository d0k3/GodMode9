#pragma once

#include "virtual.h"

void DeinitVDisaDiffDrive(void); // This is when the ivfc hash fixing actually happens - **MUST** be called before just powering off
u64 InitVDisaDiffDrive(void);
u64 CheckVDisaDiffDrive(void);

bool ReadVDisaDiffDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVDisaDiffFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
int WriteVDisaDiffFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count);