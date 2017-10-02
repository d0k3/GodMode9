#include "vtickdb.h"
#include "image.h"
#include "ticket.h"

#define VFLAG_UNKNOWN       (1UL<<29)
#define VFLAG_ESHOP         (1UL<<30)
#define VFLAG_SYSTEM        (1UL<<31)
#define VFLAG_TICKDIR       (VFLAG_UNKNOWN|VFLAG_ESHOP|VFLAG_SYSTEM)

#define NAME_TIK            "%016llX.%08lX.tik" // title id / console id

typedef struct {
    u32 commonkey_idx;
    u32 offset;
    u8  title_id[8];
    u8  titlekey[16];
    u8  ticket_id[8];
    u8  console_id[4];
    u8  eshop_id[4];
} __attribute__((packed)) TickDbEntry;

typedef struct {
    u32 n_entries;
    u8  reserved[12];
    TickDbEntry entries[256]; // this number is only a placeholder
} __attribute__((packed)) TickDbInfo;

// only for the main directory
static const VirtualFile vTickDbFileTemplates[] = {
    { "system"           , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_SYSTEM },
    { "eshop"            , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_ESHOP },
    { "unknown"          , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_UNKNOWN }
};

static TickDbInfo* tick_info     = (TickDbInfo*) VGAME_BUFFER; // full 1MB reserved (enough for ~20000 entries)

u32 AddTickDbInfo(TickDbInfo* info, Ticket* ticket, u32 offset) {
    if (ValidateTicket(ticket) != 0) return 1;
    
    // build ticket entry
    TickDbEntry* entry = info->entries + info->n_entries;
    entry->commonkey_idx = ticket->commonkey_idx;
    entry->offset = offset;
    memcpy(entry->title_id, ticket->title_id, 8);
    memcpy(entry->titlekey, ticket->titlekey, 16);
    memcpy(entry->ticket_id, ticket->ticket_id, 8);
    memcpy(entry->console_id, ticket->console_id, 4);
    memcpy(entry->eshop_id, ticket->eshop_id, 4);
    
    // check for duplicate
    u32 t = 0;
    for (; t < info->n_entries; t++) {
        TickDbEntry* entry0 = info->entries + t;
        if (memcmp(entry->title_id, entry0->title_id, 8) != 0) continue;
        if (!getbe64(entry0->console_id)) // replace this
            memcpy(entry0, entry, sizeof(TickDbEntry));
        break;
    }
    if (t >= info->n_entries)
        info->n_entries++; // new entry
    
    return 0;
}

u32 InitVTickDbDrive(void) { // prerequisite: ticket.db mounted as image
    const u32 area_offsets[] = { TICKDB_AREA_OFFSETS };
    if (!(GetMountState() & SYS_TICKDB)) return 0;
    
    // reset internal db
    memset(tick_info, 0, 16);
    
    // parse file, sector by sector
    for (u32 p = 0; p < sizeof(area_offsets) / sizeof(u32); p++) {
        u32 offset_area = area_offsets[p];
        for (u32 i = 0; i < TICKDB_AREA_SIZE; i += (TEMP_BUFFER_SIZE - 0x200)) {
            u32 read_bytes = min(TEMP_BUFFER_SIZE, TICKDB_AREA_SIZE - i);
            u8* data = (u8*) TEMP_BUFFER;
            if (ReadImageBytes(data, offset_area + i, read_bytes) != 0) {
                tick_info->n_entries = 0;
                return 0;
            }
            for (; data + TICKET_SIZE < ((u8*) TEMP_BUFFER) + read_bytes; data += 0x200) {
                Ticket* ticket = TicketFromTickDbChunk(data, NULL, false);
                if (!ticket) continue;
                AddTickDbInfo(tick_info, ticket, offset_area + i + (data - ((u8*) TEMP_BUFFER)) + 0x18);
            }
        }
    }
    
    return (tick_info->n_entries) ? SYS_TICKDB : 0;
}

u32 CheckVTickDbDrive(void) {
    if ((GetMountState() & SYS_TICKDB) && tick_info->n_entries) // very basic sanity check
        return SYS_TICKDB;
    return 0;
}

bool ReadVTickDbDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (vdir->flags & VFLAG_TICKDIR) { // ticket dir
        while (++vdir->index < (int) tick_info->n_entries) {
            TickDbEntry* tick_entry = tick_info->entries + vdir->index;
            
            u64 ticket_id = getbe64(tick_entry->ticket_id);
            u32 ck_idx = tick_entry->commonkey_idx;
            if (!(((vdir->flags & VFLAG_ESHOP) && ticket_id && (ck_idx == 0)) ||
                ((vdir->flags & VFLAG_SYSTEM) && ticket_id && (ck_idx == 1)) ||
                ((vdir->flags & VFLAG_UNKNOWN) && (!ticket_id || (ck_idx >= 2)))))
                continue;
            
            memset(vfile, 0, sizeof(VirtualFile));
            snprintf(vfile->name, 32, NAME_TIK, getbe64(tick_entry->title_id), getbe32(tick_entry->console_id));
            vfile->offset = tick_entry->offset;
            vfile->size = sizeof(Ticket);
            vfile->keyslot = 0xFF;
            vfile->flags = (vdir->flags | VFLAG_READONLY) & ~VFLAG_DIR;
            
            return true; // found
        }
    } else { // root dir
        int n_templates = sizeof(vTickDbFileTemplates) / sizeof(VirtualFile);
        const VirtualFile* templates = vTickDbFileTemplates;
        while (++vdir->index < n_templates) {
            // copy current template to vfile
            memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
            vfile->flags |= VFLAG_READONLY;
            return true; // found
        }
    }
    
    return false;
}

int ReadVTickDbFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    u64 foffset = vfile->offset + offset;
    return (ReadImageBytes(buffer, foffset, count) == 0) ? 0 : 1;
}

u64 GetVTickDbDriveSize(void) {
    return (tick_info->n_entries) ? GetMountSize() : 0;
}
