#pragma once

#include "common.h"

u32 CheckEmbeddedBackup(const char* path);
u32 EmbedEssentialBackup(const char* path);
u32 ValidateNandDump(const char* path);
u32 SafeRestoreNandDump(const char* path);
