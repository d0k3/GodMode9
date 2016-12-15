#pragma once

#include "common.h"

u32 VerifyGameFile(const char* path);
u32 CheckEncryptedGameFile(const char* path);
u32 DecryptGameFile(const char* path, bool inplace);
u32 BuildCiaFromGameFile(const char* path, bool force_legit);
