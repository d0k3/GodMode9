#pragma once

#include "common.h"

#define IMG_FAT    1
#define IMG_NAND   2

#define GAME_CIA   3
#define GAME_NCSD  4
#define GAME_NCCH  5

u32 IdentifyFileType(const char* path);
