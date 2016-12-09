#pragma once

#include "common.h"

#define IMG_FAT    (1<<0)
#define IMG_NAND   (1<<1)

#define GAME_CIA   (1<<2)
#define GAME_NCSD  (1<<3)
#define GAME_NCCH  (1<<4)
#define GAME_TMD   (1<<5)
// #define GAME_EXEFS (1<<6)
// #define GAME_ROMFS (1<<7)

#define FTYPE_MOUNTABLE     (IMG_FAT|IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH)
#define FYTPE_VERIFICABLE   (GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_TMD)

u32 IdentifyFileType(const char* path);
