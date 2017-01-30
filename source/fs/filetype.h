#pragma once

#include "common.h"

#define IMG_FAT     (1<<0)
#define IMG_NAND    (1<<1)
#define GAME_CIA    (1<<2)
#define GAME_NCSD   (1<<3)
#define GAME_NCCH   (1<<4)
#define GAME_TMD    (1<<5)
#define GAME_EXEFS  (1<<6)
#define GAME_ROMFS  (1<<7)
#define SYS_FIRM    (1<<8)
#define BIN_NCCHNFO (1<<9)
#define BIN_LAUNCH  (1<<10)

#define FLAG_CXI    (1<<30)
#define FLAG_ENCRYPTED (1<<31)

#define FTYPE_MOUNTABLE(tp)     (tp&(IMG_FAT|IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_EXEFS|GAME_ROMFS|SYS_FIRM))
#define FYTPE_VERIFICABLE(tp)   (tp&(IMG_NAND|GAME_CIA|GAME_NCSD|GAME_NCCH|GAME_TMD|SYS_FIRM))
#define FYTPE_DECRYPTABLE(tp)   (tp&(GAME_CIA|GAME_NCSD|GAME_NCCH|SYS_FIRM))
#define FTYPE_BUILDABLE(tp)     (tp&(GAME_NCSD|GAME_NCCH|GAME_TMD))
#define FTYPE_BUILDABLE_L(tp)   (FTYPE_BUILDABLE(tp) && (tp&(GAME_TMD)))
#define FTYPE_HSINJECTABLE(tp)  ((tp&(GAME_NCCH|FLAG_CXI|FLAG_ENCRYPTED)) ==  (GAME_NCCH|FLAG_CXI))
#define FTYPE_RESTORABLE(tp)    (tp&(IMG_NAND))
#define FTYPE_XORPAD(tp)        (tp&(BIN_NCCHNFO))
#define FTYPE_PAYLOAD(tp)       (tp&(BIN_LAUNCH))

u32 IdentifyFileType(const char* path);
