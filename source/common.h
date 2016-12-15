#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define vu8 volatile u8
#define vu16 volatile u16
#define vu32 volatile u32
#define vu64 volatile u64

#define max(a,b) \
    (((a) > (b)) ? (a) : (b))
#define min(a,b) \
    (((a) < (b)) ? (a) : (b))
#define getbe16(d) \
    (((d)[0]<<8) | (d)[1])
#define getbe32(d) \
    ((((u32) getbe16(d))<<16) | ((u32) getbe16(d+2)))
#define getbe64(d) \
    ((((u64) getbe32(d))<<32) | ((u64) getbe32(d+4)))
#define getle16(d) \
    (((d)[1]<<8) | (d)[0])
#define getle32(d) \
    ((((u32) getle16(d+2))<<16) | ((u32) getle16(d)))
#define getle64(d) \
    ((((u64) getle32(d+4))<<32) | ((u64) getle32(d)))
#define align(v,a) \
    (((v) % (a)) ? ((v) + (a) - ((v) % (a))) : (v))
    
// GodMode9 version
#define VERSION "0.8.7"

// input / output paths
#define INPUT_PATHS     "0:", "0:/files9", "0:/Decrypt9"
#define OUTPUT_PATH     "0:/gm9out"

// buffer area defines (in use by godmode.c)
#define DIR_BUFFER          (0x21000000)
#define DIR_BUFFER_SIZE     (0x100000)
// buffer area defines (temporary, in use by various functions)
//  -> godmode.c hexviewer
//  -> ncch.c seed setup
//  -> cia.c ticket / titlekey setup
//  -> gameutil.c various temporary stuff
#define TEMP_BUFFER         ((u8*)0x21100000)
#define TEMP_BUFFER_SIZE    (0x100000)
// buffer area defines (in use by fsutil.c, fsinit.c and gameutil.c)
#define MAIN_BUFFER         ((u8*)0x21200000)
#define MAIN_BUFFER_SIZE    (0x100000) // must be multiple of 0x200
// buffer area defines (in use by nand.c)
#define NAND_BUFFER         ((u8*)0x21300000)
#define NAND_BUFFER_SIZE    (0x100000) // must be multiple of 0x200
// buffer area defines (in use by sddata.c)
#define SDCRYPT_BUFFER      ((u8*)0x21400000)
#define SDCRYPT_BUFFER_SIZE (0x100000)
// buffer area defines (in use by vgame.c)
#define VGAME_BUFFER        ((u8*)0x21500000)
#define VGAME_BUFFER_SIZE   (0x200000) // 2MB, big RomFS
// buffer area defines (in use by image.c, for RAMdrive)
#define RAMDRV_BUFFER_O3DS  ((u8*)0x22200000) // in O3DS FCRAM
#define RAMDRV_SIZE_O3DS    (0x01C00000) // 28MB
#define RAMDRV_BUFFER_N3DS  ((u8*)0x28000000) // in N3DS FCRAM
#define RAMDRV_SIZE_N3DS    (0x08000000) // 128MB

inline u32 strchrcount(const char* str, char symbol) {
    u32 count = 0;
    for (u32 i = 0; str[i] != '\0'; i++) {
        if (str[i] == symbol)
            count++;
    }
    return count;
}
