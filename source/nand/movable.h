#pragma once

#include "common.h"

#define MOVABLE_MAGIC 'S', 'E', 'E', 'D', 0x00, 0x00, 0x00, 0x00

u8 GetMovableKeyY(const char* path, u8* keyY);
u8 GetMovableID0(const char* path, char* id0);