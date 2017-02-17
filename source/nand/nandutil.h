#pragma once

#include "common.h"

#define SAFE_SECTORS    0x000000 + 0x1, SECTOR_SECRET, SECTOR_SECRET + 0x1, \
                        SECTOR_FIRM0, SECTOR_CTR, 0x000000 // last one is a placeholder

u32 CheckEmbeddedBackup(const char* path);
u32 EmbedEssentialBackup(const char* path);
u32 ValidateNandDump(const char* path);
u32 SafeRestoreNandDump(const char* path);
