#pragma once

#include "common.h"
#include "ticket.h"

// There's probably a better place to put this
#define SD_TITLEDB_PATH(emu) ((emu) ? "B:/dbs/title.db" : "A:/dbs/title.db")

// https://www.3dbrew.org/wiki/Inner_FAT
// https://www.3dbrew.org/wiki/Title_Database

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
    u8 reserved2[16];
    u8 unknown[4]; // appears to not matter what's here
    u8 reserved3[44];
} __attribute__((packed)) TitleInfoEntry;

u32 GetNumTitleInfoEntries(const char* path);
u32 GetNumTickets(const char* path);
u32 ListTitleInfoEntryTitleIDs(const char* path, u8* title_ids, u32 max_title_ids);
u32 ListTicketTitleIDs(const char* path, u8* title_ids, u32 max_title_ids);
u32 ReadTitleInfoEntryFromDB(const char* path, const u8* title_id, TitleInfoEntry* tie);
u32 ReadTicketFromDB(const char* path, const u8* title_id, Ticket** ticket);
u32 RemoveTitleInfoEntryFromDB(const char* path, const u8* title_id);
u32 RemoveTicketFromDB(const char* path, const u8* title_id);
u32 AddTitleInfoEntryToDB(const char* path, const u8* title_id, const TitleInfoEntry* tie, bool replace);
u32 AddTicketToDB(const char* path, const u8* title_id, const Ticket* ticket, bool replace);
