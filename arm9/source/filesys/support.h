#pragma once

#include "common.h"

// scripts / payloads / markdown dir names
#define SCRIPTS_DIR     "scripts"
#define PAYLOADS_DIR    "payloads"
#define MDFILES_DIR	"markdown"

bool CheckSupportFile(const char* fname);
size_t LoadSupportFile(const char* fname, void* buffer, size_t max_len);
bool SaveSupportFile(const char* fname, void* buffer, size_t len);
bool SetAsSupportFile(const char* fname, const char* source);

bool CheckSupportDir(const char* fpath);
bool FileSelectorSupport(char* result, const char* text, const char* dname, const char* pattern);
