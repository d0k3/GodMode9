#pragma once

#include "common.h"
#include "virtual.h"

u32 InitVCartDrive(void);
#define DSPICO_IMAGE_PATH "C:/DSpico SD"
const char* GetVCartDSpicoImagePath(void);
int ReadVCartDSpicoBytes(void* buffer, u64 offset, u64 count);
int WriteVCartDSpicoBytes(const void* buffer, u64 offset, u64 count);
u64 GetVCartDSpicoSize(void);
bool ReadVCartDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVCartFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
int WriteVCartFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count);
u64 GetVCartDriveSize(void);
void GetVCartTypeString(char* typestr);
