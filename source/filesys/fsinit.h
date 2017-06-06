#pragma once

#include "common.h"
#include "ff.h"

// init SD card filesystem - required(?) for everything else
bool InitSDCardFS();

// init fill external fileystem 
bool InitExtFS();

// mount and init image file system
bool InitImgFS(const char* path);

// deinitialize external filesystem
void DeinitExtFS();

// deinitialize SD card filesystem
void DeinitSDCardFS();

// dismount drives of a certain type
void DismountDriveType(u32 type);

// returns the mount state of the SD card 
bool CheckSDMountState(void);

// get number of mounted file system (only for FATFS filesystems)
int GetMountedFSNum(const char* path);

// get mounted file system object (only for FATFS filesystems)
FATFS* GetMountedFSObject(const char* path);
