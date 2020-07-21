#pragma once

#include "common.h"
#include "ticket.h"
#include "tie.h"

// https://www.3dbrew.org/wiki/Inner_FAT

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
