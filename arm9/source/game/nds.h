#pragma once

#include "common.h"
#include "region.h"

// size of the icon struct:
// see: http://problemkaputt.de/gbatek.htm#dscartridgeicontitle
// v0x0001 -> 0x0840 byte (contains JPN, USA, FRE, GER, ITA, ESP titles)
// v0x0002 -> 0x0940 byte (adds CHN title)
// v0x0003 -> 0x0A40 byte (adds KOR title) 
// v0x0103 -> 0x23C0 byte (adds TWL animated icon data)
#define TWLICON_SIZE_DATA(v) ((v == 0x0001) ? 0x0840 : (v == 0x0002) ? 0x0940 : \
                              (v == 0x0003) ? 0x1240 : (v == 0x0103) ? 0x23C0 : 0x0000)
#define TWLICON_SIZE_DESC   128
#define TWLICON_DIM_ICON    32
#define TWLICON_SIZE_ICON   (TWLICON_DIM_ICON * TWLICON_DIM_ICON * 3) // w * h * bpp (rgb888)
#define NDS_LOGO_CRC16      0xCF56

#define TWL_UNITCODE_NTR    0x00
#define TWL_UNITCODE_TWLNTR 0x02
#define TWL_UNITCODE_TWL    0x03

// see: http://problemkaputt.de/gbatek.htm#dscartridgeicontitle
typedef struct {
    u16 version;
    u16 crc_0x0020_0x0840;
    u16 crc_0x0020_0x0940;
    u16 crc_0x0020_0x0A40;
    u16 crc_0x1240_0x23C0;
    u8  reserved[0x16];
    u8  icon[0x200]; // 32x32x4bpp / 4x4 tiles
    u16 palette[0x10]; // palette[0] is transparent
    u16 title_jap[0x80];
    u16 title_eng[0x80];
    u16 title_fre[0x80];
    u16 title_ger[0x80];
    u16 title_ita[0x80];
    u16 title_spa[0x80];
    u16 title_chn[0x80];
    u16 title_kor[0x80];
    u16 title_reserved[0x8 * 0x80];
    u8  icon_anim[0x200 * 0x8]; // 32x32x4bpp / 8 frames
    u16 palette_anim[0x10 * 0x8]; // 8 frames
    u16 sequence_anim[0x40];
} __attribute__((packed)) TwlIconData;

// very limited, information taken from here:
// https://github.com/devkitPro/ndstool/blob/dsi-support/source/header.h
// http://problemkaputt.de/gbatek.htm#dscartridgeheader
// http://problemkaputt.de/gbatek.htm#dsicartridgeheader
typedef struct {
    // common stuff (DS + DSi)
    char game_title[12];
    char game_code[4];
    char maker_code[2];
    u8  unit_code; // (0x00=NDS, 0x02=NDS+DSi, 0x03=DSi)
    u8  seed_select;
    u8  device_capacity; // cartridge size: (128 * 1024) << this
    u8  reserved0[7];
    u8  dsi_flags;
    u8  nds_region;
    u8  rom_version;
    u8  autostart; // bit2: skip "press button" after Health & Safety
    u32 arm9_rom_offset;
    u32 arm9_entry_address;
    u32 arm9_ram_address;
    u32 arm9_size;
    u32 arm7_rom_offset;
    u32 arm7_entry_address;
    u32 arm7_ram_address;
    u32 arm7_size;
    u32 fnt_offset;
    u32 fnt_size;
    u32 fat_offset;
    u32 fat_size;
    u32 arm9_overlay_offset;
    u32 arm9_overlay_size;
    u32 arm7_overlay_offset;
    u32 arm7_overlay_size;
    u32 rom_control_normal; // 0x00416657 for OneTimePROM
    u32 rom_control_key1; // 0x081808F8 for OneTimePROM
    u32 icon_offset;
    u16 secure_area_crc;
    u16 secure_area_delay;
    u32 arm9_auto_load;
    u32 arm7_auto_load;
    u64 secure_area_disable;
    u32 ntr_rom_size; // in byte
    u32 header_size;
    u8  reserved1[56];
    u8  logo[156];
    u16 logo_crc;
    u16 header_crc;
    u8  debugger_reserved[0x20];
    // extended mode stuff (DSi only)
    u8  ignored0[0x30]; // ignored
    u32 region_flags;
    u8  ignored1[0xC]; // ignored
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

u32 ValidateTwlHeader(TwlHeader* twl);
u32 LoadTwlMetaData(const char* path, TwlHeader* hdr, TwlIconData* icon);
u32 GetTwlTitle(char* desc, const TwlIconData* twl_icon);
u32 GetTwlIcon(u8* icon, const TwlIconData* twl_icon);

u32 FindNitroRomDir(u32 dirid, u32* fileid, u8** fnt_entry, TwlHeader* hdr, u8* fnt, u8* fat);
u32 NextNitroRomEntry(u32* fileid, u8** fnt_entry);
u32 ReadNitroRomEntry(u64* offset, u64* size, bool* is_dir, u32 fileid, u8* fnt_entry, u8* fat);
