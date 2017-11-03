#pragma once

#include "common.h"
    
#define GBAVC_MAGIC         '.', 'C', 'A', 'A'
#define AGBSAVE_MAGIC       '.', 'S', 'A', 'V'
#define AGBSAVE_MAX_SIZE    (0x000180 * 0x200) // standard size of the NAND partition
#define AGBSAVE_MAX_SSIZE   (AGBSAVE_MAX_SIZE - sizeof(AgbSaveHeader))

// see: http://3dbrew.org/wiki/3DS_Virtual_Console#Footer
#define GBASAVE_EEPROM_512  (512)
#define GBASAVE_EEPROM_8K   (8 * 1024)
#define GBASAVE_SRAM_32K    (32 * 1024)
#define GBASAVE_FLASH_64K   (64 * 1024)
#define GBASAVE_FLASH_128K  (128 * 1024)

#define GBASAVE_SIZE(tp) \
   (((tp == 0x0) || (tp == 0x1)) ? GBASAVE_EEPROM_512 : \
    ((tp == 0x2) || (tp == 0x3)) ? GBASAVE_EEPROM_8K  : \
    ((tp >= 0x4) && (tp <= 0x9)) ? GBASAVE_FLASH_64K  : \
    ((tp >= 0xA) && (tp <= 0xD)) ? GBASAVE_FLASH_128K : \
    (tp == 0xE) ? GBASAVE_SRAM_32K : 0); // last one means invalid
    
#define GBASAVE_VALID(size) \
   (((size) == GBASAVE_EEPROM_512) || \
    ((size) == GBASAVE_EEPROM_8K)  || \
    ((size) == GBASAVE_SRAM_32K)   || \
    ((size) == GBASAVE_FLASH_64K)  || \
    ((size) == GBASAVE_FLASH_128K))
    
// see: http://problemkaputt.de/gbatek.htm#gbacartridgeheader
#define AGB_DESTSTR(code) \
   (((code)[3] == 'J') ? "Japan"            : \
    ((code)[3] == 'E') ? "USA/English"      : \
    ((code)[3] == 'P') ? "Europe/Elsewhere" : \
    ((code)[3] == 'D') ? "German"           : \
    ((code)[3] == 'F') ? "French"           : \
    ((code)[3] == 'I') ? "Italian"          : \
    ((code)[3] == 'S') ? "Spanish" : "Unknown")   

    
// see: http://3dbrew.org/wiki/3DS_Virtual_Console#Footer
// still a lot of unknowns in here, also redundant stuff left out
typedef struct {
    u8  unknown0[4];
    u32 rom_size;
    u32 save_type;
    u8  unknown1[20];
    u32 lcd_ghosting;
    u8  video_lut[0x300];
    u8  unknown2[44];
    u8  magic[4]; // ".CAA"
    u8  unknown3[12];
} __attribute__((packed)) AgbVcFooter;

// see: http://3dbrew.org/wiki/3DS_Virtual_Console#NAND_Savegame
typedef struct {
    u8  magic[4]; // ".SAV"
    u8  reserved0[0xC]; // always 0xFF
    u8  cmac[0x10];
    u8  reserved1[0x10]; // always 0xFF
    u32 unknown0; // always 0x01
    u32 times_saved;
    u64 title_id;
    u8  sd_cid[0x10];
    u32 save_start; // always 0x200
    u32 save_size;
    u8  reserved2[0x8]; // always 0xFF
    u32 unknown1; // has to do with ARM7?
    u32 unknown2; // has to do with ARM7?
    u8  reserved3[0x198]; // always 0xFF
} __attribute__((packed)) AgbSaveHeader;

// see: http://problemkaputt.de/gbatek.htm#gbacartridgeheader
typedef struct {
    u32 arm7_rom_entry; 
    u8 logo[0x9C];
    char game_title[12];
    char game_code[4];
    char maker_code[2];
    u8 fixed;        // 0x96, required!
    u8 unit_code;    // 0x00 for current GBA
    u8 device_type;  // 0x00 usually
    u8 reserved0[7]; // always 0x00
    u8 software_version; // 0x00 usually
    u8 checksum;     // header checksum, required
    u8 reserved[2];  // always 0x00
    // stuff for multiboot not included
} __attribute__((packed)) AgbHeader;


u32 ValidateAgbSaveHeader(AgbSaveHeader* header);
u32 ValidateAgbHeader(AgbHeader* agb);
