#pragma once

#include "common.h"

// scripts / payloads dir names
#define SCRIPTS_DIR     "scripts"
#define PAYLOADS_DIR    "payloads"

bool CheckSupportFile(const char* fname);
size_t LoadSupportFile(const char* fname, void* buffer, size_t max_len);

bool CheckSupportDir(const char* fpath);
bool FileSelectorSupport(char* result, const char* text, const char* dname, const char* pattern);
