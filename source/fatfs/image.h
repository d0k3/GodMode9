#pragma once

#include "common.h"
#include "filetype.h"

int ReadImageBytes(void* buffer, u64 offset, u64 count);
int WriteImageBytes(const void* buffer, u64 offset, u64 count);
int ReadImageSectors(void* buffer, u32 sector, u32 count);
int WriteImageSectors(const void* buffer, u32 sector, u32 count);
int SyncImage(void);

u64 GetMountSize(void);
u32 GetMountState(void);
const char* GetMountPath(void);
u32 MountImage(const char* path);
