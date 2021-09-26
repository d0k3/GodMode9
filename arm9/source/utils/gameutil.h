#pragma once

#include "common.h"

u32 VerifyGameFile(const char* path);
u32 CheckEncryptedGameFile(const char* path);
u32 CryptGameFile(const char* path, bool inplace, bool encrypt);
u32 BuildCiaFromGameFile(const char* path, bool force_legit);
u32 InstallGameFile(const char* path, bool to_emunand);
u32 InstallTicketFile(const char* path, bool to_emunand);
u32 DumpTicketForGameFile(const char* path, bool force_legit);
u32 DumpCxiSrlFromTmdFile(const char* path);
u32 ExtractCodeFromCxiFile(const char* path, const char* path_out, char* extstr);
u32 CompressCode(const char* path, const char* path_out);
u64 GetGameFileTrimmedSize(const char* path);
u32 TrimGameFile(const char* path);
u32 ShowGameFileIcon(const char* path, u16* screen);
u32 ShowGameCheckerInfo(const char* path);
u64 GetGameFileTitleId(const char* path);
u32 UninstallGameDataTie(const char* path, bool remove_tie, bool remove_ticket, bool remove_save);
u32 GetTmdContentPath(char* path_content, const char* path_tmd);
u32 GetTieContentPath(char* path_content, const char* path_tie);
u32 BuildNcchInfoXorpads(const char* destdir, const char* path);
u32 CheckHealthAndSafetyInject(const char* hsdrv);
u32 InjectHealthAndSafety(const char* path, const char* destdrv);
u32 BuildTitleKeyInfo(const char* path, bool dec, bool dump);
u32 BuildSeedInfo(const char* path, bool dump);
u32 GetGoodName(char* name, const char* path, bool quick);
