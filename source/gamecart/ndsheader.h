#pragma once

#include "common.h"

// very limited, information taken from here:
// https://github.com/devkitPro/ndstool/blob/dsi-support/source/header.h
typedef struct {
    // common stuff (DS + DSi)
    char game_title[12];
    char game_code[4];
    char maker_code[2];
    u8  unit_code; // (0x00=NDS, 0x02=NDS+DSi, 0x03=DSi)
    u8  seed_select;
    u8  device_capacity; // cartridge size: (128 * 1024) << this
    u8  reserved0[7];
    u8  unknown0[2];
    u8  rom_version;
    u8  flags;
    u8  ignored0[0x60]; // ignored
    u32 ntr_rom_size; // in byte
    u32 header_size;
    u8  reserved1[56];
    u8  logo[156];
    u16 logo_crc;
    u16 header_crc;
    u8  debugger_reserved[0x20];
    // extended mode stuff (DSi only)
    u8  ignored1[0x40]; // ignored
    u32 arm9i_rom_offset;
    u32 reserved2;
    u32 arm9i_load_adress;
    u32 arm9i_size;
    u32 arm7i_rom_offset;
    u32 unknown1;
    u32 arm7i_load_adress;
    u32 arm7i_size;
    u8  ignored2[0x30]; // ignored
    u32 ntr_twl_rom_size;
    u8  unknown2[12];
    u8  ignored3[0x10]; // ignored
    u64 title_id;
    u32 pubsav_size;
    u32 prvsav_size;
    u8  reserved3[176];
    u8  unknown3[0x10];
    u8  ignored4[0xD00]; // ignored
} __attribute__((packed)) TwlHeader;
