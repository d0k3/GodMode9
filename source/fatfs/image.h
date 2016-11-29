#pragma once

#include "common.h"
#include "filetype.h"

#define IMG_RAMDRV 100 // just so there are no conflicts with file type defines

int ReadImageBytes(u8* buffer, u32 offset, u32 count);
int WriteImageBytes(const u8* buffer, u32 offset, u32 count);
int ReadImageSectors(u8* buffer, u32 sector, u32 count);
int WriteImageSectors(const u8* buffer, u32 sector, u32 count);
int SyncImage(void);

u64 GetMountSize(void);
u32 GetMountState(void);
const char* GetMountPath(void);
u32 MountRamDrive(void);
u32 MountImage(const char* path);
