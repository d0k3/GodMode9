#pragma once

#include "common.h"

#define HSINJECT_MARKFILE "__gm9_hsbak.pth"

u32 VerifyGameFile(const char* path);
u32 CheckEncryptedGameFile(const char* path);
u32 CryptGameFile(const char* path, bool inplace, bool encrypt);
u32 BuildCiaFromGameFile(const char* path, bool force_legit);
u32 BuildNcchInfoXorpads(const char* destdir, const char* path);
u32 CheckHealthAndSafetyInject(const char* hsdrv);
u32 InjectHealthAndSafety(const char* path, const char* destdrv);
