#include "vtickdb.h"
#include "image.h"
#include "disadiff.h"
#include "ticketdb.h"
#include "ui.h" // this takes long - we need a way to keep the user in check

#define VTICKDB_BUFFER_SIZE 0x100000 // 1MB, enough for ~20000 entries

#define VFLAG_HIDDEN        (1UL<<28)
#define VFLAG_UNKNOWN       (1UL<<29)
#define VFLAG_ESHOP         (1UL<<30)
#define VFLAG_SYSTEM        (1UL<<31)
#define VFLAG_TICKDIR       (VFLAG_HIDDEN|VFLAG_UNKNOWN|VFLAG_ESHOP|VFLAG_SYSTEM)
#define OFLAG_RAW           (1lu << 31)

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
    TickDbEntry entries[256]; // this number is only a placeholder (dangerous?)
} __attribute__((packed)) TickDbInfo;

// only for the main directory
static const VirtualFile vTickDbFileTemplates[] = {
    { "system"           , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_SYSTEM },
    { "eshop"            , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_ESHOP },
    { "unknown"          , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_UNKNOWN },
    { "hidden"           , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_HIDDEN }
};


static TickDbInfo* tick_info = NULL;
static u8* lvl2_cache = NULL;
static DisaDiffReaderInfo diff_info;
static bool scanned_raw = false;


u32 AddTickDbInfo(TickDbInfo* info, Ticket* ticket, u32 offset, bool replace) {
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
        if (replace && !getbe32(entry0->console_id)) // replace this
            memcpy(entry0, entry, sizeof(TickDbEntry));
        break;
    }
    if (t >= info->n_entries)
        info->n_entries++; // new entry
    
    return 0;
}

void ScanTickDb(bool raw_mode, bool replace) {
    // set up buffer
    u8* data = (u8*) malloc(TICKDB_AREA_SIZE);
    if (!data) return;
    
    if (!raw_mode) { // proper DIFF decoding
        // read and decode ticket.db DIFF partition
        ShowString("Loading DIFF data...");
        if (ReadDisaDiffIvfcLvl4(NULL, &diff_info, TICKDB_AREA_OFFSET, TICKDB_AREA_SIZE, data) == TICKDB_AREA_SIZE) {
            // parse the decoded data for valid tickets
            for (u32 i = 0; i < TICKDB_AREA_SIZE + 0x400; i += 0x200) {
                if (!(i % 0x10000) && !ShowProgress(i, TICKDB_AREA_SIZE, "Scanning for tickets")) break;
                Ticket* ticket = TicketFromTickDbChunk(data + i, NULL, true);
                if (!ticket) continue;
                AddTickDbInfo(tick_info, ticket, TICKDB_AREA_OFFSET + i + 0x18, replace);
            }
        }
    } else { // scan RAW data
        const u32 area_offsets[] = { TICKDB_AREA_RAW };
        for (u32 p = 0; p < sizeof(area_offsets) / sizeof(u32); p++) {
            u32 offset_area = area_offsets[p];
            ShowString("Loading raw data (%lu)...", p);
            if (ReadImageBytes(data, offset_area, TICKDB_AREA_SIZE) != 0)
                continue;
            for (u32 i = 0; i < TICKDB_AREA_SIZE + 0x400; i += 0x200) {
                if (!(i % 0x10000) && !ShowProgress(i, TICKDB_AREA_SIZE, "Scanning for tickets")) break;
                Ticket* ticket = TicketFromTickDbChunk(data + i, NULL, true);
                if (!ticket) continue;
                AddTickDbInfo(tick_info, ticket, (offset_area + i + 0x18) | OFLAG_RAW, replace);
            }
        }
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    free(data);
}

void DeinitVTickDbDrive(void) {
    if (tick_info) free(tick_info);
    if (lvl2_cache) free(lvl2_cache);
    tick_info = NULL;
    lvl2_cache = NULL;
    scanned_raw = false;
}

u64 InitVTickDbDrive(void) { // prerequisite: ticket.db mounted as image
    if (!(GetMountState() & SYS_TICKDB)) return 0;
    
    // set up drive buffer / internal db
    DeinitVTickDbDrive();
    tick_info = (TickDbInfo*) malloc(VTICKDB_BUFFER_SIZE);
    if (!tick_info) return 0;
    memset(tick_info, 0, 16);
    
    // setup DIFF reading
    if ((GetDisaDiffReaderInfo(NULL, &diff_info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(diff_info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &diff_info, lvl2_cache, diff_info.size_dpfs_lvl2) != 0)) {
        DeinitVTickDbDrive();
        return 0;
    }
    
    ScanTickDb(false, true);
    if (!tick_info->n_entries) DeinitVTickDbDrive();
    return (tick_info->n_entries) ? SYS_TICKDB : 0;
}

u64 CheckVTickDbDrive(void) {
    if ((GetMountState() & SYS_TICKDB) && tick_info) // very basic sanity check
        return SYS_TICKDB;
    return 0;
}

bool ReadVTickDbDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (vdir->flags & VFLAG_TICKDIR) { // ticket dir
        // raw scan required?
        if ((vdir->flags & VFLAG_HIDDEN) && !scanned_raw) {
            ScanTickDb(true, false);
            scanned_raw = true;
        }
        
        while (++vdir->index < (int) tick_info->n_entries) {
            TickDbEntry* tick_entry = tick_info->entries + vdir->index;
            
            u64 ticket_id = getbe64(tick_entry->ticket_id);
            u32 ck_idx = tick_entry->commonkey_idx;
            bool hidden = tick_entry->offset & OFLAG_RAW;
            if (hidden && !(vdir->flags & VFLAG_HIDDEN)) continue;
            if (!(((vdir->flags & VFLAG_HIDDEN) && hidden) ||
                ((vdir->flags & VFLAG_ESHOP) && ticket_id && (ck_idx == 0)) ||
                ((vdir->flags & VFLAG_SYSTEM) && ticket_id && (ck_idx == 1)) ||
                ((vdir->flags & VFLAG_UNKNOWN) && (!ticket_id || (ck_idx >= 2)))))
                continue;
            
            memset(vfile, 0, sizeof(VirtualFile));
            snprintf(vfile->name, 32, NAME_TIK, getbe64(tick_entry->title_id), getbe32(tick_entry->console_id));
            vfile->offset = tick_entry->offset & ~OFLAG_RAW;
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
    u64 foffset = vfile->offset+ offset;
    bool hidden = vfile->flags & VFLAG_HIDDEN;
    if (hidden) return (ReadImageBytes(buffer, foffset, count) == 0) ? 0 : 1;
    else return (ReadDisaDiffIvfcLvl4(NULL, &diff_info, (u32) foffset, (u32) count, buffer) == (u32) count) ? 0 : 1;
}

u64 GetVTickDbDriveSize(void) {
    return (tick_info->n_entries) ? GetMountSize() : 0;
}
