#pragma once

#include "common.h"
#include "keydb.h"

u32 CryptAesKeyDb(const char* path, bool inplace, bool encrypt);
u32 AddKeyToDb(AesKeyInfo* key_info, AesKeyInfo* key_entry);
u32 BuildKeyDb(const char* path, bool dump);
