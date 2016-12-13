#pragma once

#include "common.h"
#include "filetype.h"

int ReadImageBytes(u8* buffer, u64 offset, u64 count);
int WriteImageBytes(const u8* buffer, u64 offset, u64 count);
int ReadImageSectors(u8* buffer, u32 sector, u32 count);
int WriteImageSectors(const u8* buffer, u32 sector, u32 count);
int SyncImage(void);

u64 GetMountSize(void);
u32 GetMountState(void);
const char* GetMountPath(void);
u32 MountImage(const char* path);
