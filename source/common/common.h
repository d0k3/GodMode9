#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <types.h>
#include <stdalign.h>

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
#define countof(x) \
    (sizeof(x) / sizeof((x)[0]))

#define STATIC_ASSERT(...) \
    _Static_assert((__VA_ARGS__), #__VA_ARGS__)

// GodMode9 / SafeMode9 ("flavor" / splash screen)
#ifndef SAFEMODE
#define FLAVOR "GodMode9"
#define QLZ_SPLASH_H "gm9_splash_qlz.h"
#define QLZ_SPLASH gm9_splash_qlz
#else
#define FLAVOR "SafeMode9"
#define QLZ_SPLASH_H "sm9_splash_baby_qlz.h"
#define QLZ_SPLASH sm9_splash_baby_qlz
#endif

// input / output paths
#define SUPPORT_PATH    "0:/gm9/support"
#define SCRIPT_PATH     "0:/gm9/scripts"
#define PAYLOAD_PATH    "0:/gm9/payloads"
#define OUTPUT_PATH     "0:/gm9/out"

// buffer area defines (in use by godmode.c)
#define DIR_BUFFER          (0x20000000)
#define DIR_BUFFER_SIZE     (0x100000)
// buffer area defines (in use by fsutil.c, fsinit.c and gameutil.c)
#define MAIN_BUFFER         ((u8*)0x20100000)
#define MAIN_BUFFER_SIZE    (0x100000) // must be multiple of 0x200
// buffer area defines (in use by nand.c)
#define NAND_BUFFER         ((u8*)0x20200000)
#define NAND_BUFFER_SIZE    (0x100000) // must be multiple of 0x200
// buffer area defines (in use by sddata.c)
#define SDCRYPT_BUFFER      ((u8*)0x20300000)
#define SDCRYPT_BUFFER_SIZE (0x100000)
// buffer area defines (in use by scripting.c)
#define SCRIPT_BUFFER       ((u8*)0x20400000)
#define SCRIPT_BUFFER_SIZE  (0x100000)
// buffer area defines (in use by vgame.c)
#define VGAME_BUFFER        ((u8*)0x20500000)
#define VGAME_BUFFER_SIZE   (0x200000) // 2MB, big RomFS
// buffer area defines (in use by vcart.c)
#define VCART_BUFFER        ((u8*)0x20700000)
#define VCART_BUFFER_SIZE   (0x20000) // 128kB, this is more than enough

// buffer area defines (temporary, in use by various functions)
//  -> godmode.c hexviewer
//  -> godmode.c loading payloads
//  -> ncch.c seed setup
//  -> cia.c ticket / titlekey setup
//  -> gameutil.c various temporary stuff
//  -> nandcmac.c for processing agbsave
//  -> nandutil.c for storing essential backup
//  -> ctrtransfer.c for SecureInfo (temporary)
//  -> vgame.c for handling FIRMs
//  -> vtickdb.c for parsing ticket.db
//  -> qlzcomp.c for temporary compression stuff
//  -> codelzss.c for decompressing .code
// meaning: careful when using this!
#define TEMP_BUFFER         ((u8*)0x20800000)
#define TEMP_BUFFER_SIZE    (0x400000) // 4MB
#define TEMP_BUFFER_EXTSIZE (0x1800000) // 24MB(!) (only used by codelzss.c right now)

// buffer area defines (in use by image.c, for RAMdrive)
#define RAMDRV_BUFFER       ((u8*)0x22800000) // top of STACK
#define RAMDRV_SIZE_O3DS    (0x5800000) // 88MB
#define RAMDRV_SIZE_N3DS    (0xD800000) // 216MB
