#pragma once

#include "common.h"

u32 VerifyNcchFile(const char* path, u32 offset, u32 size);
u32 VerifyNcsdFile(const char* path);
u32 VerifiyCiaFile(const char* path);
u32 VerifyGameFile(const char* path);

u32 DecryptNcchFile(const char* path, u32 offset, u32 size);
u32 DecryptNcsdFile(const char* path);
u32 DecryptCiaFile(const char* path, bool deep);
