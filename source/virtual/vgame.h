#pragma once

#include "common.h"
#include "filetype.h"
#include "virtual.h"

u32 InitVGameDrive(void);
u32 CheckVGameDrive(void);

bool OpenVGameDir(VirtualDir* vdir, VirtualFile* ventry);
bool ReadVGameDir(VirtualFile* vfile, VirtualDir* vdir);
int ReadVGameFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count);
// int WriteVGameFile(const VirtualFile* vfile, const u8* buffer, u32 offset, u32 count); // writing is not enabled

bool FindVirtualFileInLv3Dir(VirtualFile* vfile, const VirtualDir* vdir, const char* name);
bool GetVGameLv3Filename(char* name, const VirtualFile* vfile, u32 n_chars);
bool MatchVGameLv3Filename(const char* name, const VirtualFile* vfile, u32 n_chars);
