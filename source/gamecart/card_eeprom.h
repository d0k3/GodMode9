#include "ndscard.h"

//---------------------------------------------------------------------------------
static inline void eepromWaitBusy() {
//---------------------------------------------------------------------------------
	while (REG_AUXSPICNT & CARD_SPI_BUSY);
}

// Reads from the EEPROM
void cardReadEeprom(u32 address, u8 *data, u32 length, u32 addrtype);

// Writes to the EEPROM. TYPE 3 EEPROM must be erased first (I think?)
void cardWriteEeprom(u32 address, u8 *data, u32 length, u32 addrtype);

// Returns the ID of the EEPROM chip? Doesn't work well, most chips give ff,ff
// i = 0 or 1
u32 cardEepromReadID();

// Sends a command to the EEPROM
u8 cardEepromCommand(u8 command);

/*
 * -1:no card or no EEPROM
 *  0:unknown                 PassMe?
 *  1:TYPE 1  4Kbit(512Byte)  EEPROM
 *  2:TYPE 2 64Kbit(8KByte)or 512kbit(64Kbyte)   EEPROM
 *  3:TYPE 3  2Mbit(256KByte) FLASH MEMORY (some rare 4Mbit and 8Mbit chips also)
 */
int cardEepromGetType(void);

// Returns the size in bytes of EEPROM
u32 cardEepromGetSize();

// Erases the entire chip. TYPE 3 chips MUST be erased before writing to them. (I think?)
void cardEepromChipErase(void);

// Erases a single sector of the TYPE 3 chip
void cardEepromSectorErase(u32 address);
