#pragma once

#include "common.h"

#define VAR_BUFFER_SIZE (72 * 1024) // enough for exactly 256 vars

#define SCRIPT_EXT      "gm9"
#define SCRIPT_MAX_SIZE (SCRIPT_BUFFER_SIZE-VAR_BUFFER_SIZE-1)

bool ValidateText(const char* text, u32 size);
bool MemTextViewer(const char* text, u32 len, bool as_script);
bool FileTextViewer(const char* path, bool as_script);
bool ExecuteGM9Script(const char* path_script);
