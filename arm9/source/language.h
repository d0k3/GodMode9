#pragma once

#include "common.h"

#define STRING(what, def) extern const char* STR_##what;
#include "language.inl"
#undef STRING

bool SetLanguage(const void* translation, u32 translation_size);
const void* GetLanguage(const void* riff, u32 riff_size, u32* version, u32* count, char* language_name);

bool LanguageMenu(char* result, const char* title);
