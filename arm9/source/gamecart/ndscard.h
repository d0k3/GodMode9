
// Copyright 2014 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// modifyed by osilloscopion (2 Jul 2016)
//

#pragma once
#include <inttypes.h>
#include "delay.h"

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define vu8 volatile u8
#define vu16 volatile u16
#define vu32 volatile u32
#define vu64 volatile u64

#define REG_ROMCTRL			(*(vu32*)0x10164004)
#define REG_AUXSPICNT		(*(vu16*)0x10164000)
#define REG_AUXSPICNTH		(*(vu16*)0x10164001)
#define REG_AUXSPIDATA		(*(vu16*)0x10164002)

#define CARD_COMMAND		((vu8*)0x10164008)
#define CARD_DATA_RD		(*(vu32*)0x1016401C)

#define CARD_CR1_ENABLE  0x8000u
#define CARD_CR1_IRQ     0x4000u

// SPI EEPROM COMMANDS
#define SPI_EEPROM_WRSR   0x01
#define SPI_EEPROM_PP     0x02	// Page Program
#define SPI_EEPROM_READ   0x03
#define SPI_EEPROM_WRDI   0x04  // Write disable
#define SPI_EEPROM_RDSR   0x05  // Read status register
#define SPI_EEPROM_WREN   0x06  // Write enable
#define SPI_EEPROM_PW     0x0a	// Page Write
#define SPI_EEPROM_FAST   0x0b	// Fast Read
#define SPI_EEPROM_RDID   0x9f
#define SPI_EEPROM_RDP    0xab	// Release from deep power down
#define SPI_EEPROM_DPD    0xb9  // Deep power down

#define CARD_ACTIVATE     (1u<<31)           // when writing, get the ball rolling
#define CARD_WR           (1u<<30)           // Card write enable
#define CARD_nRESET       (1u<<29)           // value on the /reset pin (1 = high out, not a reset state, 0 = low out = in reset)
#define CARD_SEC_LARGE    (1u<<28)           // Use "other" secure area mode, which tranfers blocks of 0x1000 bytes at a time
#define CARD_CLK_SLOW     (1u<<27)           // Transfer clock rate (0 = 6.7MHz, 1 = 4.2MHz)
#define CARD_BLK_SIZE(n)  (((n)&0x7u)<<24)   // Transfer block size, (0 = None, 1..6 = (0x100 << n) bytes, 7 = 4 bytes)
#define CARD_SEC_CMD      (1u<<22)           // The command transfer will be hardware encrypted (KEY2)
#define CARD_DELAY2(n)    (((n)&0x3Fu)<<16)  // Transfer delay length part 2
#define CARD_SEC_SEED     (1u<<15)           // Apply encryption (KEY2) seed to hardware registers
#define CARD_SEC_EN       (1u<<14)           // Security enable
#define CARD_SEC_DAT      (1u<<13)           // The data transfer will be hardware encrypted (KEY2)
#define CARD_DELAY1(n)    ((n)&0x1FFFu)      // Transfer delay length part 1

// 3 bits in b10..b8 indicate something
// read bits
#define CARD_BUSY         (1u<<31)           // when reading, still expecting incomming data?
#define CARD_DATA_READY   (1u<<23)           // when reading, REG_NTRCARDFIFO has another word of data and is good to go

// Card commands
#define CARD_CMD_DUMMY          0x9Fu
#define CARD_CMD_HEADER_READ    0x00u
#define CARD_CMD_HEADER_CHIPID  0x90u
#define CARD_CMD_ACTIVATE_BF    0x3Cu  // Go into blowfish (KEY1) encryption mode
#define CARD_CMD_ACTIVATE_BF2   0x3Du  // Go into blowfish (KEY1) encryption mode
#define CARD_CMD_ACTIVATE_SEC   0x40u  // Go into hardware (KEY2) encryption mode
#define CARD_CMD_SECURE_CHIPID  0x10u
#define CARD_CMD_SECURE_READ    0x20u
#define CARD_CMD_DISABLE_SEC    0x60u  // Leave hardware (KEY2) encryption mode
#define CARD_CMD_DATA_MODE      0xA0u
#define CARD_CMD_DATA_READ      0xB7u
#define CARD_CMD_DATA_CHIPID    0xB8u

//REG_AUXSPICNT
#define CARD_ENABLE			(1<<15)
#define CARD_SPI_ENABLE		(1<<13)
#define CARD_SPI_BUSY		(1<<7)
#define CARD_SPI_HOLD		(1<<6)

#define CARD_SPICNTH_ENABLE  (1<<7)  // in byte 1, i.e. 0x8000
#define CARD_SPICNTH_IRQ     (1<<6)  // in byte 1, i.e. 0x4000

#define swiDelay(n) ioDelay(n)

#define DMA_SRC(n)		(*(vu32*)(0x10002004 + (n * 0x1c)))
#define DMA_DEST(n)		(*(vu32*)(0x10002008 + (n * 0x1c)))
#define DMA_CR(n)		(*(vu32*)(0x1000201C + (n * 0x1c)))

#define DMA_ENABLE		(1u << 31)
#define DMA_START_CARD	(5u << 27)
#define DMA_32_BIT		(1u << 26)
#define DMA_REPEAT		(1u << 25)
#define DMA_SRC_FIX		(1u << 24)

void cardReset();
