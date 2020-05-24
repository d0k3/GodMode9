#pragma once

#include "common.h"
#include "virtual.h"

void DeinitVBDRIDrive(void);
u64 InitVBDRIDrive(void);
u64 CheckVBDRIDrive(void);

bool ReadVBDRIDir(VirtualFile* vfile, VirtualDir* vdir);
bool GetNewVBDRIFile(VirtualFile* vfile, VirtualDir* vdir, const char* path);
int ReadVBDRIFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
int WriteVBDRIFile(VirtualFile* vfile, const void* buffer, u64 offset, u64 count);
int DeleteVBDRIFile(const VirtualFile* vfile);
u64 GetVBDRIDriveSize(void);
