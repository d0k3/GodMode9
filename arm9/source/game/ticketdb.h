#pragma once

#include "common.h"
#include "ticket.h"

#define TIKDB_NAME_ENC      "encTitleKeys.bin"
#define TIKDB_NAME_DEC      "decTitleKeys.bin"
#define TIKDB_SIZE(tdb)     (16 + ((tdb)->n_entries * sizeof(TitleKeyEntry)))

#define TICKDB_PATH(emu)    ((emu) ? "4:/dbs/ticket.db" : "1:/dbs/ticket.db") // EmuNAND / SysNAND
#define TICKDB_AREA_OFFSET  0xA1C00 // offset inside the decoded DIFF partition
#define TICKDB_AREA_RAW     0x0137F000, 0x001C0C00 // raw offsets inside the file
#define TICKDB_AREA_SIZE    0x00500000 // 5MB, arbitrary (around 1MB is realistic)

#define TICKDB_MAGIC        0x44, 0x49, 0x46, 0x46, 0x00, 0x00, 0x03, 0x00, \
                            0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                            0x30, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                            0x2C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                            0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                            0x00, 0xEE, 0x37, 0x02, 0x00, 0x00, 0x00, 0x00


typedef struct {
    u32 commonkey_idx;
    u8  reserved[4];
    u8  title_id[8];
    u8  titlekey[16];
} __attribute__((packed)) TitleKeyEntry;

typedef struct {
    u32 n_entries;
    u8  reserved[12];
    TitleKeyEntry entries[256]; // this number is only a placeholder
} __attribute__((packed)) TitleKeysInfo;


u32 GetTitleKey(u8* titlekey, Ticket* ticket);
Ticket* TicketFromTickDbChunk(u8* chunk, u8* title_id, bool legit_pls);
u32 FindTicket(Ticket* ticket, u8* title_id, bool force_legit, bool emunand);
u32 FindTitleKey(Ticket* ticket, u8* title_id);
u32 AddTitleKeyToInfo(TitleKeysInfo* tik_info, TitleKeyEntry* tik_entry, bool decrypted_in, bool decrypted_out, bool devkit);
u32 AddTicketToInfo(TitleKeysInfo* tik_info, Ticket* ticket, bool decrypt);
