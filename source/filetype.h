#pragma once

#include "common.h"

#define IMG_FAT    1
#define IMG_NAND   2

u32 IdentifyFileType(const char* path);
