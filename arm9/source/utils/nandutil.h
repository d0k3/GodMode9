#pragma once

#include "common.h"

u32 CheckEmbeddedBackup(const char* path);
u32 EmbedEssentialBackup(const char* path);
u32 FixNandHeader(const char* path, bool check_size);
u32 ValidateNandDump(const char* path);
u32 SafeRestoreNandDump(const char* path);
u32 SafeInstallFirm(const char* path, u32 slots);
u32 SafeInstallKeyDb(const char* path);
u32 DumpGbaVcSavegame(const char* path);
u32 InjectGbaVcSavegame(const char* path, const char* path_vcsave);
