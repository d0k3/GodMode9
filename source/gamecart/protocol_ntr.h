// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common.h"

#define REG_NTRCARDMCNT    (*(vu16*)0x10164000)
#define REG_NTRCARDMDATA   (*(vu16*)0x10164002)
#define REG_NTRCARDROMCNT  (*(vu32*)0x10164004)
#define REG_NTRCARDCMD     ((vu8*)0x10164008)
#define REG_NTRCARDSEEDX_L (*(vu32*)0x10164010)
#define REG_NTRCARDSEEDY_L (*(vu32*)0x10164014)
#define REG_NTRCARDSEEDX_H (*(vu16*)0x10164018)
#define REG_NTRCARDSEEDY_H (*(vu16*)0x1016401A)
#define REG_NTRCARDFIFO    (*(vu32*)0x1016401C)

#define NTRCARD_PAGESIZE_0   (0<<24)
#define NTRCARD_PAGESIZE_4   (7u<<24)
#define NTRCARD_PAGESIZE_512 (1u<<24)
#define NTRCARD_PAGESIZE_1K  (2u<<24)
#define NTRCARD_PAGESIZE_2K  (3u<<24)
#define NTRCARD_PAGESIZE_4K  (4u<<24)
#define NTRCARD_PAGESIZE_8K  (5u<<24)
#define NTRCARD_PAGESIZE_16K (6u<<24)

#define NTRCARD_ACTIVATE     (1u<<31)           // when writing, get the ball rolling
#define NTRCARD_WR           (1u<<30)           // Card write enable
#define NTRCARD_nRESET       (1u<<29)           // value on the /reset pin (1 = high out, not a reset state, 0 = low out = in reset)
#define NTRCARD_SEC_LARGE    (1u<<28)           // Use "other" secure area mode, which tranfers blocks of 0x1000 bytes at a time
#define NTRCARD_CLK_SLOW     (1u<<27)           // Transfer clock rate (0 = 6.7MHz, 1 = 4.2MHz)
#define NTRCARD_BLK_SIZE(n)  (((n)&0x7u)<<24)   // Transfer block size, (0 = None, 1..6 = (0x100 << n) bytes, 7 = 4 bytes)
#define NTRCARD_SEC_CMD      (1u<<22)           // The command transfer will be hardware encrypted (KEY2)
#define NTRCARD_DELAY2(n)    (((n)&0x3Fu)<<16)  // Transfer delay length part 2
#define NTRCARD_SEC_SEED     (1u<<15)           // Apply encryption (KEY2) seed to hardware registers
#define NTRCARD_SEC_EN       (1u<<14)           // Security enable
#define NTRCARD_SEC_DAT      (1u<<13)           // The data transfer will be hardware encrypted (KEY2)
#define NTRCARD_DELAY1(n)    ((n)&0x1FFFu)      // Transfer delay length part 1

// 3 bits in b10..b8 indicate something
// read bits
#define NTRCARD_BUSY         (1u<<31)           // when reading, still expecting incomming data?
#define NTRCARD_DATA_READY   (1u<<23)           // when reading, REG_NTRCARDFIFO has another word of data and is good to go

// Card commands
#define NTRCARD_CMD_DUMMY          0x9Fu
#define NTRCARD_CMD_HEADER_READ    0x00u
#define NTRCARD_CMD_HEADER_CHIPID  0x90u
#define NTRCARD_CMD_ACTIVATE_BF    0x3Cu  // Go into blowfish (KEY1) encryption mode
#define NTRCARD_CMD_ACTIVATE_BF2   0x3Du  // Go into blowfish (KEY1) encryption mode
#define NTRCARD_CMD_ACTIVATE_SEC   0x40u  // Go into hardware (KEY2) encryption mode
#define NTRCARD_CMD_SECURE_CHIPID  0x10u
#define NTRCARD_CMD_SECURE_READ    0x20u
#define NTRCARD_CMD_DISABLE_SEC    0x60u  // Leave hardware (KEY2) encryption mode
#define NTRCARD_CMD_DATA_MODE      0xA0u
#define NTRCARD_CMD_DATA_READ      0xB7u
#define NTRCARD_CMD_DATA_CHIPID    0xB8u

#define NTRCARD_CR1_ENABLE  0x8000u
#define NTRCARD_CR1_IRQ     0x4000u

#define NTRKEY_PARAM 0x3F1FFFu

void NTR_SendCommand(const u32 command[2], u32 pageSize, u32 latency, void* buffer);
