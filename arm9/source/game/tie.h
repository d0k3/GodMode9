#pragma once

#include "common.h"
#include "tmd.h"
#include "ncch.h"
#include "nds.h"

// There's probably a better place to put this
#define SD_TITLEDB_PATH(emu) ((emu) ? "B:/dbs/title.db" : "A:/dbs/title.db")


// see: https://www.3dbrew.org/wiki/Title_Database
typedef struct {
    u64 title_size;
    u32 title_type; // usually == 0x40
    u32 title_version;
    u8 flags_0[4];
    u32 tmd_content_id;
    u32 cmd_content_id;
    u8 flags_1[4];
    u32 extdata_id_low; // 0 if the title doesn't use extdata
    u8 reserved1[4];
    u8 flags_2[8];
    char product_code[16];
    u8 reserved2[12];
    u32 content0_id; // only relevant for TWL?
    u8 unknown[4]; // appears to not matter what's here
    u8 reserved3[44];
} __attribute__((packed)) TitleInfoEntry;

u32 BuildTitleInfoEntryTwl(TitleInfoEntry* tie, TitleMetaData* tmd, TwlHeader* twl);
u32 BuildTitleInfoEntryNcch(TitleInfoEntry* tie, TitleMetaData* tmd, NcchHeader* ncch, NcchExtHeader* exthdr, bool sd);
