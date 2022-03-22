#include "vbdri.h"
#include "image.h"
#include "disadiff.h"
#include "vdisadiff.h"
#include "bdri.h"
#include "vff.h"
#include "ui.h"
#include "language.h"

#define VBDRI_MAX_ENTRIES   8192 // Completely arbitrary

#define VFLAG_UNKNOWN       (1UL<<28)
#define VFLAG_ILLEGIT       (1UL<<29)
#define VFLAG_ESHOP         (1UL<<30)
#define VFLAG_SYSTEM        (1UL<<31)
#define VFLAG_TICKDIR       (VFLAG_UNKNOWN|VFLAG_ILLEGIT|VFLAG_ESHOP|VFLAG_SYSTEM)

#define NAME_TIE            "%016llX"
#define NAME_TIE_LEN        16
#define NAME_TIK            "%016llX.%08lX.tik" // title id / console id
#define NAME_TIK_LEN        (16 + 1 + 8 + 4)

#define PART_PATH           "D:/partitionA.bin"


typedef struct {
    u8 type; // 0 for eshop, 1 for homebrew, 2 for system, 3 for unknown
    u32 size;
    u8  console_id[4];
} PACKED_STRUCT TickInfoEntry;

// only for the main directory
static const VirtualFile VTickDbFileTemplates[] = {
    { "eshop"   , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_ESHOP },
    { "system"  , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_SYSTEM },
    { "unknown" , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_UNKNOWN },
    { "illegit" , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_ILLEGIT },
};

static bool is_tickdb;
static u32 num_entries = 0;
static u8* title_ids = NULL;
static TickInfoEntry* tick_info = NULL;
static u8* cached_entry = NULL;
static int cache_index;

void DeinitVBDRIDrive(void) {
    free(title_ids);
    free(tick_info);
    free(cached_entry);
    title_ids = NULL;
    tick_info = NULL;
    cached_entry = NULL;
    num_entries = 0;
    cache_index = -1;
}

bool SortVBDRITickets() {
    if (!CheckVBDRIDrive() || !is_tickdb)
        return false;

    if (tick_info)
        return true;

    tick_info = (TickInfoEntry*) malloc(num_entries * sizeof(TickInfoEntry));
    if (!tick_info)
        return false;

    ShowString("%s", STR_SORTING_TICKETS_PLEASE_WAIT);

    for (u32 i = 0; i < num_entries - 1; i++) {
        Ticket* ticket;
        if (ReadTicketFromDB(PART_PATH, title_ids + (i * 8), &ticket) != 0) {
            free(tick_info);
            tick_info = NULL;
            return false;
        }
        tick_info[i].type =
            (ValidateTicketSignature(ticket) != 0) ? 3 : // illegit
            (ticket->commonkey_idx > 1) ? 2 : // unknown
            ticket->commonkey_idx; // eshop (0) / system (1)
        tick_info[i].size = GetTicketSize(ticket);
        memcpy(tick_info[i].console_id, ticket->console_id, 4);
        free(ticket);
    }

    ClearScreenF(true, false, COLOR_STD_BG);

    return true;
}

u64 InitVBDRIDrive(void) { // prerequisite: .db file mounted as virtual diff image
    u64 mount_state = CheckVDisaDiffDrive();
    if (!(mount_state & SYS_DIFF)) return 0;
    is_tickdb = (mount_state & SYS_TICKDB);

    DeinitVBDRIDrive();

    num_entries = min((is_tickdb ? GetNumTickets(PART_PATH) : GetNumTitleInfoEntries(PART_PATH)) + 1, VBDRI_MAX_ENTRIES);
    title_ids = (u8*) malloc(num_entries * 8);
    if (!title_ids ||
        ((is_tickdb ? ListTicketTitleIDs(PART_PATH, title_ids, num_entries) : ListTitleInfoEntryTitleIDs(PART_PATH, title_ids, num_entries)) != 0)) {
        DeinitVBDRIDrive();
        return 0;
    }

    if (!is_tickdb && ((cached_entry = malloc(sizeof(TitleInfoEntry))) == NULL)) {
        DeinitVBDRIDrive();
        return 0;
    }

    return mount_state;
}

u64 CheckVBDRIDrive(void) {
    u64 mount_state = CheckVDisaDiffDrive();
    return (title_ids && (mount_state & SYS_DIFF) && (!is_tickdb || (mount_state & SYS_TICKDB))) ?
        mount_state : 0;
}

bool ReadVBDRIDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (!CheckVBDRIDrive())
        return false;

    if (vdir->flags & VFLAG_TICKDIR) { // ticket dir
        if (!is_tickdb || (!tick_info && !SortVBDRITickets()))
            return false;

        while (++vdir->index < (int) num_entries) {
            u32 type = tick_info[vdir->index].type;
            u64 tid = getbe64(title_ids + (vdir->index * 8));

            if ((tid == 0) || !(
                ((vdir->flags & VFLAG_ESHOP)   && (type == 0)) ||
                ((vdir->flags & VFLAG_SYSTEM)  && (type == 1)) ||
                ((vdir->flags & VFLAG_UNKNOWN) && (type == 2)) ||
                ((vdir->flags & VFLAG_ILLEGIT) && (type == 3))))
                continue;

            memset(vfile, 0, sizeof(VirtualFile));
            snprintf(vfile->name, 32, NAME_TIK, tid, getle32(tick_info[vdir->index].console_id));
            vfile->offset = vdir->index; // "offset" is the internal buffer index
            vfile->size = tick_info[vdir->index].size;
            vfile->keyslot = 0xFF;
            vfile->flags = (vdir->flags | VFLAG_DELETABLE) & ~VFLAG_DIR;

            return true; // found
        }
    } else { // root dir
        if (is_tickdb) {
            int n_templates = sizeof(VTickDbFileTemplates) / sizeof(VirtualFile);
            const VirtualFile* templates = VTickDbFileTemplates;
            while (++vdir->index < n_templates) {
                // copy current template to vfile
                memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
                return true; // found
            }
        } else { // title dbs display all entries in root
            while (++vdir->index < (int) num_entries) {
                u64 tid = getbe64(title_ids + (vdir->index * 8));
                if (tid == 0)
                    continue;
                memset(vfile, 0, sizeof(VirtualFile));
                snprintf(vfile->name, 32, NAME_TIE, tid);
                vfile->offset = vdir->index; // "offset" is the internal buffer index
                vfile->size = sizeof(TitleInfoEntry);
                vfile->keyslot = 0xFF;
                vfile->flags = (vdir->flags | VFLAG_DELETABLE) & ~VFLAG_DIR;

                return true;
            }
        }
    }

    return false;
}

bool GetNewVBDRIFile(VirtualFile* vfile, VirtualDir* vdir, const char* path) {

    size_t path_len = strlen(path), buf_len = (is_tickdb ? NAME_TIK_LEN : NAME_TIE_LEN) + 2;
    u64 tid;
    u32 console_id;
    char c;

    char buf[buf_len];
    strcpy(buf, path + path_len - buf_len + 1);


    if (( is_tickdb && (sscanf(buf, "/" NAME_TIK "%c", &tid, &console_id, &c) != 2)) ||
        (!is_tickdb && (sscanf(buf, "/" NAME_TIE "%c", &tid, &c) != 1)) ||
        (tid == 0))
        return false;
    tid = getbe64((u8*)&tid);

    int entry_index = -1;
    for (u32 i = 0; i < num_entries; i++) {
        if ((entry_index == -1) && (*((u64*)(void*)(title_ids + 8 * i)) == 0))
            entry_index = i;

        if (memcmp(&tid, title_ids + 8 * i, 8) == 0)
            return false;
    }

    if (entry_index == -1) {
        if (num_entries == VBDRI_MAX_ENTRIES)
            return false;

        u32 new_num_entries = min(num_entries + 128, VBDRI_MAX_ENTRIES);
        u8* new_title_ids = realloc(title_ids, new_num_entries * 8);
        if (!new_title_ids)
            return false;
        if (tick_info) {
            TickInfoEntry* new_tick_info = realloc(tick_info, new_num_entries * sizeof(TickInfoEntry));
            if (!new_tick_info)
                return false;
            tick_info = new_tick_info;
        }

        entry_index = num_entries;
        num_entries = new_num_entries;
        title_ids = new_title_ids;

        memset(title_ids + entry_index * 8, 0, (num_entries - entry_index) * 8);
    }

    u32 size = is_tickdb ? TICKET_COMMON_SIZE : sizeof(TitleInfoEntry);
    u8 entry[size];
    if (is_tickdb)
        *((u32*)(void*)(entry + 0x2A8)) = 0xAC000000;
    if ((is_tickdb ? AddTicketToDB(PART_PATH, (u8*)&tid, (Ticket*)(void*)entry, false) :
        AddTitleInfoEntryToDB(PART_PATH, (u8*)&tid, (TitleInfoEntry*)(void*)entry, false)) != 0)
        return false;

    memcpy(title_ids + entry_index * 8, &tid, 8);

    if (tick_info) {
        tick_info[entry_index].type = 3;
        tick_info[entry_index].size = TICKET_COMMON_SIZE;
        memset(tick_info[entry_index].console_id, 0, 4);
    }

    memset(vfile, 0, sizeof(VirtualFile));
    strcpy(vfile->name, buf);
    vfile->offset = entry_index; // "offset" is the internal buffer index
    vfile->size = size;
    vfile->keyslot = 0xFF;
    vfile->flags = (vdir->flags | VFLAG_DELETABLE) & ~VFLAG_DIR;

    return true;
}

int ReadVBDRIFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    if ((int) vfile->offset == cache_index) {
        memcpy(buffer, cached_entry + offset, count);
        return 0;
    }

    if (is_tickdb && (cache_index != -1))
        free(cached_entry);
    if ((is_tickdb ? ReadTicketFromDB(PART_PATH, title_ids + vfile->offset * 8, (Ticket**) &cached_entry) :
        ReadTitleInfoEntryFromDB(PART_PATH, title_ids + vfile->offset * 8, (TitleInfoEntry*) cached_entry)) != 0)
        return 1;
    cache_index = (int) vfile->offset;

    memcpy(buffer, cached_entry + offset, count);
    return 0;
}

int WriteVBDRIFile(VirtualFile* vfile, const void* buffer, u64 offset, u64 count) {
    bool resize = false;

    if (offset + count > vfile->size) {
        if (!is_tickdb || (!tick_info && !SortVBDRITickets()))
            return false;
        vfile->size = offset + count;
        resize = true;
    }

    if (is_tickdb && (cache_index != -1))
        free(cached_entry);
    if ((is_tickdb ? ReadTicketFromDB(PART_PATH, title_ids + vfile->offset * 8, (Ticket**) &cached_entry) :
        ReadTitleInfoEntryFromDB(PART_PATH, title_ids + vfile->offset * 8, (TitleInfoEntry*) cached_entry)) != 0) {
        if (resize) vfile->size = tick_info[vfile->offset].size;
        return 1;
    }
    cache_index = (int) vfile->offset;

    if (resize) {
        u8* new_cached_entry = realloc(cached_entry, vfile->size);
        if (!new_cached_entry) {
            vfile->size = tick_info[vfile->offset].size;
            return 1;
        }

        cached_entry = new_cached_entry;

        if (RemoveTicketFromDB(PART_PATH, title_ids + vfile->offset * 8) != 0) {
            vfile->size = tick_info[vfile->offset].size;
            return 1;
        }
    }

    memcpy(cached_entry + offset, buffer, count);

    if ((is_tickdb ? AddTicketToDB(PART_PATH, title_ids + vfile->offset * 8, (Ticket*)(void*)cached_entry, true) :
        AddTitleInfoEntryToDB(PART_PATH, title_ids + vfile->offset * 8, (TitleInfoEntry*)(void*)cached_entry, true)) != 0) {
        if (resize) vfile->size = tick_info[vfile->offset].size;
        if (is_tickdb) {
            free(cached_entry);
            cached_entry = NULL;
        }
        cache_index = -1;
        return 1;
    }

    if (resize) tick_info[vfile->offset].size = vfile->size;

    if (tick_info && ((offset <= 0x1F1 && offset + count > 0x1F1) || (cached_entry[0x1F1] == 0 && offset <= 0x104 && offset + count > 4)))
        tick_info[vfile->offset].type = (cached_entry[0x1F1] > 1) ? 3 :
            ((ValidateTicketSignature((Ticket*)(void*)cached_entry) != 0) ? 1 : ((cached_entry[0x1F1] == 1) ? 2 : 0));

    return 0;
}

int DeleteVBDRIFile(const VirtualFile* vfile) {
    int ret = (int) (is_tickdb ? RemoveTicketFromDB(PART_PATH, title_ids + vfile->offset * 8) : RemoveTitleInfoEntryFromDB(PART_PATH, title_ids + vfile->offset * 8));

    if (ret == 0) {
        if ((int) vfile->offset == cache_index) {
            cache_index = -1;
            if (is_tickdb) {
                free(cached_entry);
                cached_entry = NULL;
            }
        }

        memset(title_ids + vfile->offset * 8, 0, 8);
    }

    return ret;
}

u64 GetVBDRIDriveSize(void) {
    return CheckVBDRIDrive() ? fvx_qsize(PART_PATH) : 0;
}
