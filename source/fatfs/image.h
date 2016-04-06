#pragma once

#include "common.h"

#define IMG_FAT  1
#define IMG_NAND 2

int ReadImageSectors(u8* buffer, u32 sector, u32 count);
int WriteImageSectors(const u8* buffer, u32 sector, u32 count);
int SyncImage(void);

u64 GetMountSize(void);
u32 GetMountState(void);
u32 IdentifyImage(const char* path);
u32 MountImage(const char* path);
