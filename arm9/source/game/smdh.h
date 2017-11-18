#pragma once

#include "common.h"
#include "region.h"

#define SMDH_MAGIC 'S', 'M', 'D', 'H'
#define SMDH_SIZE_DESC_SHORT    64
#define SMDH_SIZE_DESC_LONG     128
#define SMDH_SIZE_PUBLISHER     64
#define SMDH_DIM_ICON_SMALL     24
#define SMDH_DIM_ICON_BIG       48
#define SMDH_SIZE_ICON_SMALL    (SMDH_DIM_ICON_SMALL * SMDH_DIM_ICON_SMALL * 3) // w * h * bpp (rgb888)
#define SMDH_SIZE_ICON_BIG      (SMDH_DIM_ICON_BIG * SMDH_DIM_ICON_BIG * 3) // w * h * bpp (rgb888)

// see: https://www.3dbrew.org/wiki/SMDH#Application_Titles
typedef struct {
    u16 short_desc[0x40];
    u16 long_desc[0x80];
    u16 publisher[0x40];
} __attribute__((packed)) SmdhAppTitle;

// see: https://www.3dbrew.org/wiki/SMDH
typedef struct {
    char magic[4];
    u16 version;
    u16 reserved0;
    SmdhAppTitle apptitles[0x10]; // 1 -> english title
    u8  game_ratings[0x10];
    u32 region_lockout;
    u32 matchmaker_id;
    u64 matchmaker_id_bit;
    u32 flags;
    u16 version_eula;
    u16 reserved1;
    u32 anim_def_frame;
    u32 cec_id;
    u64 reserved2;
    u16 icon_small[0x240]; // 24x24x16bpp / 8x8 tiles / rgb565
    u16 icon_big[0x900];  // 48x48x16bpp / 8x8 tiles / rgb565
} __attribute__((packed)) Smdh;

u32 GetSmdhDescShort(char* desc, const Smdh* smdh);
u32 GetSmdhDescLong(char* desc, const Smdh* smdh);
u32 GetSmdhPublisher(char* pub, const Smdh* smdh);
u32 GetSmdhIconSmall(u8* icon, const Smdh* smdh);
u32 GetSmdhIconBig(u8* icon, const Smdh* smdh);
