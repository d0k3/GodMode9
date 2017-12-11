#pragma once

#include "common.h"
#include "exefs.h"

#define ESSENTIAL_NAME  "essential.exefs"

// magic number for essential backup
#define ESSENTIAL_MAGIC 'n', 'a', 'n', 'd', '_', 'h', 'd', 'r', 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00

// size of /ro/sys/HWCAL0.dat and /ro/sys/HWCAL1.dat
#define SIZE_HWCAL 0x9D0

// /rw/sys/LocalFriendCodeSeed_B (/_A) file
// see: http://3dbrew.org/wiki/Nandrw/sys/LocalFriendCodeSeed_B
typedef struct {
    u8 signature[0x100];
    u8 unknown[0x8]; // normally zero
    u8 codeseed[0x8]; // the actual data
} __attribute__((packed)) LocalFriendCodeSeed;

// /private/movable.sed file
// see: http://3dbrew.org/wiki/Nand/private/movable.sed
typedef struct {
    u8 magic[0x4]; // "SEED"
    u8 indicator[0x4]; // uninitialized all zero, otherwise u8[1] nonzero  
    LocalFriendCodeSeed codeseed_data;
    u8 keyy_high[8];
    u8 unknown[0x10];
    u8 cmac[0x10];
} __attribute__((packed)) MovableSed;

// /rw/sys/SecureInfo_A (/_B) file
// see: http://3dbrew.org/wiki/Nandrw/sys/SecureInfo_A
typedef struct {
    u8 signature[0x100];
    u8 region;
    u8 unknown;
    char serial[0xF];
} __attribute__((packed)) SecureInfo;

// includes all essential system files
// (this is of our own making)
typedef struct {
    ExeFsHeader header;
    u8 nand_hdr[0x200];
    SecureInfo secinfo;
    u8 padding_secinfo[0x200 - (sizeof(SecureInfo)%0x200)];
    MovableSed movable;
    u8 padding_movable[0x200 - (sizeof(MovableSed)%0x200)];
    LocalFriendCodeSeed frndseed;
    u8 padding_frndseed[0x200 - (sizeof(LocalFriendCodeSeed)%0x200)];
    u8 nand_cid[0x10];
    u8 padding_nand_cid[0x200 - 0x10];
    u8 otp[0x100];
    u8 padding_otp[0x200 - 0x100];
    u8 hwcal0[SIZE_HWCAL];
    u8 padding_hwcal0[0x200 - (SIZE_HWCAL%0x200)];
    u8 hwcal1[SIZE_HWCAL];
    u8 padding_hwcal1[0x200 - (SIZE_HWCAL%0x200)];
} __attribute__((packed)) EssentialBackup;
