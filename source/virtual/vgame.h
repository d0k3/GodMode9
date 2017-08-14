#pragma once

#include "common.h"
#include "filetype.h"
#include "virtual.h"

u32 InitVGameDrive(void);
u32 CheckVGameDrive(void);

bool OpenVGameDir(VirtualDir* vdir, VirtualFile* ventry);
bool ReadVGameDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVGameFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count);
// int WriteVGameFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count); // writing is not enabled

bool FindVirtualFileInLv3Dir(VirtualFile* vfile, const VirtualDir* vdir, const char* name);
bool GetVGameFilename(char* name, const VirtualFile* vfile, u32 n_chars);
bool MatchVGameFilename(const char* name, const VirtualFile* vfile, u32 n_chars);

u64 GetVGameDriveSize(void);
