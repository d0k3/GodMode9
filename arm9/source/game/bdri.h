#pragma once

#include "common.h"
#include "ticket.h"

// There's probably a better place to put this
#define SD_TITLEDB_PATH(emu) ((emu) ? "B:/dbs/title.db" : "A:/dbs/title.db")

// https://www.3dbrew.org/wiki/Inner_FAT
// https://www.3dbrew.org/wiki/Title_Database

typedef struct {
    char magic[4]; // "BDRI"
    u32 version; // == 0x30000
    u64 info_offset; // == 0x20
    u64 image_size; // in blocks; and including the pre-header
    u32 image_block_size;
    u8 padding1[4];
    u8 unknown[4];
    u32 data_block_size;
    u64 dht_offset;
    u32 dht_bucket_count;
    u8 padding2[4];
    u64 fht_offset;
    u32 fht_bucket_count;
    u8 padding3[4];
    u64 fat_offset;
    u32 fat_entry_count; // exculdes 0th entry
    u8 padding4[4];
    u64 data_offset;
    u32 data_block_count; // == fat_entry_count
    u8 padding5[4];
    u32 det_start_block;
    u32 det_block_count;
    u32 max_dir_count;
    u8 padding6[4];
    u32 fet_start_block;
    u32 fet_block_count;
    u32 max_file_count;
    u8 padding7[4];
} __attribute__((packed)) BDRIFsHeader;

typedef struct {
    char magic[8]; // varies based on media type and importdb vs titledb
    u8 reserved[0x78];
    BDRIFsHeader fs_header;
} __attribute__((packed)) TitleDBPreHeader;

typedef struct {
    char magic[4]; // "TICK"
    u32 unknown1; // usually (assuming always) == 1
    u32 unknown2;
    u32 unknown3;
    BDRIFsHeader fs_header;
} __attribute__((packed)) TickDBPreHeader;

typedef struct {
    u32 parent_index;
    u8 title_id[8];
    u32 next_sibling_index;
    u8 padding1[4];
    u32 start_block_index;
    u64 size; // in bytes
    u8 padding2[8];
    u32 hash_bucket_next_index;
} __attribute__((packed)) TdbFileEntry;

typedef struct {
    u32 total_entry_count;
    u32 max_entry_count; // == max_file_count + 1
    u8 padding[32];
    u32 next_dummy_index;
} __attribute__((packed)) DummyFileEntry;

typedef struct {
    u64 title_size; // in bytes
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

typedef struct {
    u32 unknown; // usually (assuming always) == 1
    u32 ticket_size; // commonly == 0x350
    Ticket ticket;
} __attribute__((packed, aligned(4))) TicketEntry;

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