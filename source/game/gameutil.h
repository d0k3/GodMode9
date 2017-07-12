#pragma once

#include "common.h"

u32 VerifyGameFile(const char* path);
u32 CheckEncryptedGameFile(const char* path);
u32 CryptGameFile(const char* path, bool inplace, bool encrypt);
u32 BuildCiaFromGameFile(const char* path, bool force_legit);
u32 DumpCxiSrlFromTmdFile(const char* path);
u32 ShowGameFileTitleInfo(const char* path);
u32 BuildNcchInfoXorpads(const char* destdir, const char* path);
u32 CheckTargetInject(const char* hsdrv, u32 application);
u32 InjectTarget(const char* path, const char* destdrv, u32 application);
u32 BuildTitleKeyInfo(const char* path, bool dec, bool dump);
u32 BuildSeedInfo(const char* path, bool dump);
u32 GetGoodName(char* name, const char* path, bool quick);