#include "vgame.h"
#include "image.h"
#include "game.h"
#include "aes.h"

#define VFLAG_FIRM_SECTION  (1<<22)
#define VFLAG_FIRM_ARM9     (1<<23)
#define VFLAG_FIRM          (1<<24)
#define VFLAG_EXEFS_FILE    (1<<25)
#define VFLAG_EXTHDR        (1<<26)
#define VFLAG_CIA           (1<<27)
#define VFLAG_NCSD          (1<<28)
#define VFLAG_NCCH          (1<<29)
#define VFLAG_EXEFS         (1<<30)
#define VFLAG_ROMFS         (1<<31)
#define VFLAG_GAMEDIR       (VFLAG_FIRM|VFLAG_CIA|VFLAG_NCSD|VFLAG_NCCH|VFLAG_EXEFS|VFLAG_ROMFS|VFLAG_LV3)

#define NAME_FIRM_HEADER    "header.bin"
#define NAME_FIRM_ARM9BIN   "arm9dec.bin"
#define NAME_FIRM_SECTION   "section%lu.%s.bin" // number.arm9/.arm11
#define NAME_FIRM_NCCH      "%016llX.%.8s%s" // name.title_id(.ext)

#define NAME_CIA_HEADER     "header.bin"
#define NAME_CIA_CERT       "cert.bin"
#define NAME_CIA_TICKET     "ticket.bin"
#define NAME_CIA_TMD        "tmd.bin"
#define NAME_CIA_TMDCHUNK   "tmdchunks.bin"
#define NAME_CIA_META       "meta.bin"
#define NAME_CIA_CONTENT    "%04X.%08lX%s" // index.id(.ext)

#define NAME_NCSD_HEADER    "ncsd.bin"
#define NAME_NCSD_CARDINFO  "cardinfo.bin"
#define NAME_NCSD_DEVINFO   "devinfo.bin"
#define NAME_NCSD_TYPES     "game", "manual", "dlp", \
                            "unk", "unk", "unk", \
                            "update_n3ds", "update_o3ds"
#define NAME_NCSD_CONTENT   "content%lu.%s%s" // content?.type(.ext)

#define NAME_NCCH_HEADER    "ncch.bin"
#define NAME_NCCH_EXTHEADER "extheader.bin"
#define NAME_NCCH_PLAIN     "plain.bin"
#define NAME_NCCH_LOGO      "logo.bin"
#define NAME_NCCH_EXEFS     "exefs.bin"
#define NAME_NCCH_ROMFS     "romfs.bin"
#define NAME_NCCH_EXEFSDIR  "exefs"
#define NAME_NCCH_ROMFSDIR  "romfs"


static u32 vgame_type = 0;
static u32 base_vdir = 0;

static VirtualFile* templates_cia   = (VirtualFile*) VGAME_BUFFER; // first 56kb reserved (enough for 1024 entries)
static VirtualFile* templates_firm  = (VirtualFile*) (VGAME_BUFFER + 0xE000); // 2kb reserved (enough for 36 entries)
static VirtualFile* templates_ncsd  = (VirtualFile*) (VGAME_BUFFER + 0xE800); // 2kb reserved (enough for 36 entries)
static VirtualFile* templates_ncch  = (VirtualFile*) (VGAME_BUFFER + 0xF000); // 2kb reserved (enough for 36 entries)
static VirtualFile* templates_exefs = (VirtualFile*) (VGAME_BUFFER + 0xF800); // 2kb reserved (enough for 36 entries)
static int n_templates_cia   = -1;
static int n_templates_firm  = -1;
static int n_templates_ncsd  = -1;
static int n_templates_ncch  = -1;
static int n_templates_exefs = -1;

static u64 offset_firm  = (u64) -1;
static u64 offset_a9bin = (u64) -1;
static u64 offset_cia   = (u64) -1;
static u64 offset_ncsd  = (u64) -1;
static u64 offset_ncch  = (u64) -1;
static u64 offset_exefs = (u64) -1;
static u64 offset_romfs = (u64) -1;
static u64 offset_lv3   = (u64) -1;
static u64 offset_lv3fd = (u64) -1;

static CiaStub* cia = (CiaStub*) (void*) (VGAME_BUFFER + 0x10000); // 61.5kB reserved - should be enough by far
static FirmA9LHeader* a9l = (FirmA9LHeader*) (void*) (VGAME_BUFFER + 0x1F600); // 512 byte reserved
static FirmHeader* firm = (FirmHeader*) (void*) (VGAME_BUFFER + 0x1F800); // 512 byte reserved
static NcsdHeader* ncsd = (NcsdHeader*) (void*) (VGAME_BUFFER + 0x1FA00); // 512 byte reserved
static NcchHeader* ncch = (NcchHeader*) (void*) (VGAME_BUFFER + 0x1FC00); // 512 byte reserved
static ExeFsHeader* exefs = (ExeFsHeader*) (void*) (VGAME_BUFFER + 0x1FE00); // 512 byte reserved
static u8* romfslv3 = (u8*) (VGAME_BUFFER + 0x20000); // 1920kB reserved
static RomFsLv3Index lv3idx;

int ReadFirmImageBytes(u8* buffer, u64 offset, u64 count) {
    int ret = ReadImageBytes(buffer, offset, count);
    if ((offset_a9bin == (u64) -1) || (ret != 0)) return ret;
    if (DecryptFirm(buffer, offset, count, firm, a9l) != 0)
        return -1;
    return 0;
}

int ReadNcchImageBytes(u8* buffer, u64 offset, u64 count) {
    int ret = (offset_a9bin == (u64) - 1) ? ReadImageBytes(buffer, offset, count) : 
        ReadFirmImageBytes(buffer, offset, count);
    if ((offset_ncch == (u64) -1) || (ret != 0)) return ret;
    if (NCCH_ENCRYPTED(ncch) && (DecryptNcch(buffer, offset - offset_ncch, count, ncch,
        (offset_exefs == (u64) -1) ? NULL : exefs) != 0)) return -1;
    return 0;
}

bool BuildVGameExeFsDir(void) {
    VirtualFile* templates = templates_exefs;
    u32 n = 0;
    
    for (u32 i = 0; i < 10; i++) {
        ExeFsFileHeader* file = exefs->files + i;
        if (file->size == 0) continue;
        snprintf(templates[n].name, 32, "%.8s", file->name);
        templates[n].offset = offset_exefs + sizeof(ExeFsHeader) + file->offset;
        templates[n].size = file->size;
        templates[n].keyslot = ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch)) ?
            0x2C : 0xFF; // actual keyslot may be different
        templates[n].flags = VFLAG_EXEFS_FILE;
        n++;
    }
    
    n_templates_exefs = n;
    return true;
}

bool BuildVGameNcchDir(void) {
    VirtualFile* templates = templates_ncch;
    u32 n = 0;
    
    // NCCH crypto
    bool ncch_crypto = (NCCH_ENCRYPTED(ncch)) && (SetupNcchCrypto(ncch, NCCH_NOCRYPTO) == 0);
    
    // header
    strncpy(templates[n].name, NAME_NCCH_HEADER, 32);
    templates[n].offset = offset_ncch + 0;
    templates[n].size = 0x200;
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // extended header
    if (ncch->size_exthdr) {
        strncpy(templates[n].name, NAME_NCCH_EXTHEADER, 32);
        templates[n].offset = offset_ncch + NCCH_EXTHDR_OFFSET;
        templates[n].size = NCCH_EXTHDR_SIZE;
        templates[n].keyslot = ncch_crypto ? 0x2C : 0xFF;
        templates[n].flags = VFLAG_EXTHDR;
        n++;
    }
    
    // plain region
    if (ncch->size_plain) {
        strncpy(templates[n].name, NAME_NCCH_PLAIN, 32);
        templates[n].offset = offset_ncch + (ncch->offset_plain * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_plain * NCCH_MEDIA_UNIT;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // logo region
    if (ncch->size_logo) {
        strncpy(templates[n].name, NAME_NCCH_LOGO, 32);
        templates[n].offset =offset_ncch + (ncch->offset_logo * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_logo * NCCH_MEDIA_UNIT;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // exefs
    if (ncch->size_exefs) {
        strncpy(templates[n].name, NAME_NCCH_EXEFS, 32);
        templates[n].offset = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_exefs * NCCH_MEDIA_UNIT;
        templates[n].keyslot = ncch_crypto ? 0x2C : 0xFF; // real slot may be something else
        templates[n].flags = VFLAG_EXEFS;
        n++;
        if (!NCCH_ENCRYPTED(ncch) || ncch_crypto) {
            memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
            strncpy(templates[n].name, NAME_NCCH_EXEFSDIR, 32);
            templates[n].flags |= VFLAG_DIR;
            n++;
        }
    }
    
    // romfs
    if (ncch->size_romfs) {
        strncpy(templates[n].name, NAME_NCCH_ROMFS, 32);
        templates[n].offset = offset_ncch + (ncch->offset_romfs * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_romfs * NCCH_MEDIA_UNIT;
        templates[n].keyslot = ncch_crypto ? 0x2C : 0xFF; // real slot may be something else
        templates[n].flags = VFLAG_ROMFS;
        n++;
        if (!NCCH_ENCRYPTED(ncch) || ncch_crypto) {
            memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
            strncpy(templates[n].name, NAME_NCCH_ROMFSDIR, 32);
            templates[n].flags |= VFLAG_DIR;
            n++;
        }
    }
    
    n_templates_ncch = n;
    return true;
}
    
bool BuildVGameNcsdDir(void) {
    const char* name_type[] = { NAME_NCSD_TYPES };
    VirtualFile* templates = templates_ncsd;
    u32 n = 0;
    
    // header
    strncpy(templates[n].name, NAME_NCSD_HEADER, 32);
    templates[n].offset = 0;
    templates[n].size = 0x200;
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // card info header
    if (ncsd->partitions[0].offset * NCSD_MEDIA_UNIT >= NCSD_CINFO_OFFSET + NCSD_CINFO_SIZE) {
        strncpy(templates[n].name, NAME_NCSD_CARDINFO, 32);
        templates[n].offset = NCSD_CINFO_OFFSET;
        templates[n].size = NCSD_CINFO_SIZE;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // dev info header
    if (ncsd->partitions[0].offset * NCSD_MEDIA_UNIT >= NCSD_DINFO_OFFSET + NCSD_DINFO_SIZE) {
        strncpy(templates[n].name, NAME_NCSD_DEVINFO, 32);
        templates[n].offset = NCSD_DINFO_OFFSET;
        templates[n].size = NCSD_DINFO_SIZE;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // contents
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd->partitions + i;
        if ((partition->offset == 0) && (partition->size == 0))
            continue;
        snprintf(templates[n].name, 32, NAME_NCSD_CONTENT, i, name_type[i], ".app");
        templates[n].offset = partition->offset * NCSD_MEDIA_UNIT;
        templates[n].size = partition->size * NCSD_MEDIA_UNIT;
        templates[n].keyslot = 0xFF; // not encrypted
        templates[n].flags = VFLAG_NCCH;
        n++;
        memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
        snprintf(templates[n].name, 32, NAME_NCSD_CONTENT, i, name_type[i], "");
        templates[n].flags |= VFLAG_DIR;
        n++;
    }
    
    n_templates_ncsd = n;
    return true;
}

bool BuildVGameCiaDir(void) {
    CiaInfo info;
    VirtualFile* templates = templates_cia;
    u32 n = 0;
    
    if (GetCiaInfo(&info, &(cia->header)) != 0)
        return false;
    
    // header
    strncpy(templates[n].name, NAME_CIA_HEADER, 32);
    templates[n].offset = 0;
    templates[n].size = info.size_header;
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // certificates
    if (info.size_cert) {
        strncpy(templates[n].name, NAME_CIA_CERT, 32);
        templates[n].offset = info.offset_cert;
        templates[n].size = info.size_cert;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // ticket
    if (info.size_ticket) {
        strncpy(templates[n].name, NAME_CIA_TICKET, 32);
        templates[n].offset = info.offset_ticket;
        templates[n].size = info.size_ticket;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // TMD (the full thing)
    if (info.size_tmd) {
        strncpy(templates[n].name, NAME_CIA_TMD, 32);
        templates[n].offset = info.offset_tmd;
        templates[n].size = info.size_tmd;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // TMD content chunks
    if (info.size_content_list) {
        strncpy(templates[n].name, NAME_CIA_TMDCHUNK, 32);
        templates[n].offset = info.offset_content_list;
        templates[n].size = info.size_content_list;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // meta
    if (info.size_meta) {
        strncpy(templates[n].name, NAME_CIA_META, 32);
        templates[n].offset = info.offset_meta;
        templates[n].size = info.size_meta;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // contents
    if (info.size_content) {
        TmdContentChunk* content_list = cia->content_list;
        u32 content_count = getbe16(cia->tmd.content_count);
        u64 next_offset = info.offset_content;
        for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
            u64 size = getbe64(content_list[i].size);
            bool is_ncch = false; // is unencrypted NCCH?
            if (!(getbe16(content_list[i].type) & 0x1)) {
                NcchHeader ncch;
                ReadImageBytes((u8*) &ncch, next_offset, sizeof(NcchHeader));
                is_ncch = (ValidateNcchHeader(&ncch) == 0);
            }
            snprintf(templates[n].name, 32, NAME_CIA_CONTENT,
                getbe16(content_list[i].index), getbe32(content_list[i].id), ".app");
            templates[n].offset = next_offset;
            templates[n].size = size;
            templates[n].keyslot = 0xFF; // even for encrypted stuff
            templates[n].flags = is_ncch ? VFLAG_NCCH : 0;
            n++;
            if (is_ncch) {
                memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                snprintf(templates[n].name, 32, NAME_CIA_CONTENT,
                    getbe16(content_list[i].index), getbe32(content_list[i].id), "");
                templates[n].flags |= VFLAG_DIR;
                n++;
            }
            next_offset += size;
        }
    }
    
    n_templates_cia = n;
    return true;
}

bool BuildVGameFirmDir(void) {
    VirtualFile* templates = templates_firm;
    u32 n = 0;
    
    // header
    strncpy(templates[n].name, NAME_FIRM_HEADER, 32);
    templates[n].offset = 0;
    templates[n].size = 0x200;
    templates[n].keyslot = 0xFF;
    templates[n].flags = 0;
    n++;
    
    // arm9 binary (only for encrypted)
    if (offset_a9bin != (u64) -1) {
        strncpy(templates[n].name, NAME_FIRM_ARM9BIN, 32);
        templates[n].offset = offset_a9bin;
        templates[n].size = GetArm9BinarySize(a9l);
        templates[n].keyslot = 0x15;
        templates[n].flags = VFLAG_FIRM_ARM9;
        n++;
    }
    
    // section binaries & NCCHs
    for (u32 i = 0; i < 4; i++) {
        FirmSectionHeader* section = firm->sections + i;
        if (!section->size) continue;
        snprintf(templates[n].name, 32, NAME_FIRM_SECTION, i, (section->type == 0) ? "arm9" : "arm11");
        templates[n].offset = section->offset;
        templates[n].size = section->size;
        templates[n].keyslot = 0xFF;
        templates[n].flags = VFLAG_FIRM_SECTION;
        n++;
        if (section->type == 0) { // ARM9 section, search for Process9
            u8* buffer = (u8*) (TEMP_BUFFER + (TEMP_BUFFER_SIZE/2));
            u32 buffer_size = TEMP_BUFFER_SIZE/4;
            NcchHeader* p9_ncch;
            char name[8];
            u32 offset_p9 = 0;
            for (u32 p = 0; (p < section->size) && (!offset_p9); p += buffer_size) {
                u32 btr = min(buffer_size, (section->size - p));
                if (ReadFirmImageBytes(buffer, section->offset + p, btr) != 0) break;
                for (u32 s = 0; (s < btr) && (!offset_p9); s += 0x10) {
                    p9_ncch = (NcchHeader*) (void*) (buffer + s);
                    if ((ValidateNcchHeader(p9_ncch) == 0) &&
                        (ReadFirmImageBytes((u8*) name, section->offset + p + s + 0x200, 8) == 0))
                        offset_p9 = section->offset + p + s;
                }
            }
            
            if (offset_p9) {
                snprintf(templates[n].name, 32, NAME_FIRM_NCCH, p9_ncch->programId, name, ".app");
                templates[n].offset = offset_p9;
                templates[n].size = p9_ncch->size * NCCH_MEDIA_UNIT;
                templates[n].keyslot = (offset_a9bin == (u64) -1) ? 0xFF : 0x15;
                templates[n].flags = VFLAG_NCCH;
                n++;
                memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                snprintf(templates[n].name, 32, NAME_FIRM_NCCH, p9_ncch->programId, name, "");
                templates[n].flags |= VFLAG_DIR;
                n++;
            }
        } else if (section->type == 1) { // ARM11 section, search for modules
            NcchHeader firm_ncch;
            for (u32 v = 0; v < 2; v++) {
                u32 start = v ? ARM11V2_OFFSET : 0;
                for (u32 p = start; p < section->size; p += firm_ncch.size * NCCH_MEDIA_UNIT) {
                    char name[8];
                    if ((ReadImageBytes((u8*) &firm_ncch, section->offset + p, 0x200) != 0) ||
                        (ReadImageBytes((u8*) name, section->offset + p + 0x200, 0x8) != 0) ||
                        (ValidateNcchHeader(&firm_ncch) != 0))
                        break;
                    
                    snprintf(templates[n].name, 32, NAME_FIRM_NCCH, firm_ncch.programId, name, ".app");
                    templates[n].offset = section->offset + p;
                    templates[n].size = firm_ncch.size * NCCH_MEDIA_UNIT;
                    templates[n].keyslot = 0xFF;
                    templates[n].flags = VFLAG_NCCH;
                    n++;
                    memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                    snprintf(templates[n].name, 32, NAME_FIRM_NCCH, firm_ncch.programId, name, "");
                    templates[n].flags |= VFLAG_DIR;
                    n++;
                }
            }
        }
    }
    
    n_templates_firm = n;
    return true;
}

u32 InitVGameDrive(void) { // prerequisite: game file mounted as image
    u32 type = GetMountState();
    
    vgame_type = 0;
    offset_firm  = (u64) -1;
    offset_a9bin = (u64) -1;
    offset_cia   = (u64) -1;
    offset_ncsd  = (u64) -1;
    offset_ncch  = (u64) -1;
    offset_exefs = (u64) -1;
    offset_romfs = (u64) -1;
    offset_lv3   = (u64) -1;
    offset_lv3fd = (u64) -1;
    
    base_vdir =
        (type & SYS_FIRM  ) ? VFLAG_FIRM  :
        (type & GAME_CIA  ) ? VFLAG_CIA   :
        (type & GAME_NCSD ) ? VFLAG_NCSD  :
        (type & GAME_NCCH ) ? VFLAG_NCCH  :
        (type & GAME_EXEFS) ? VFLAG_EXEFS :
        (type & GAME_ROMFS) ? VFLAG_ROMFS : 0;
    if (!base_vdir) return 0;
    
    vgame_type = type;
    return type;
}

u32 CheckVGameDrive(void) {
    if (vgame_type != GetMountState()) vgame_type = 0; // very basic sanity check
    return vgame_type;
}

bool OpenVGameDir(VirtualDir* vdir, VirtualFile* ventry) {
    // build vdir object
    vdir->index = -1;
    if (!ventry) { // root dir
        vdir->offset = 0;
        vdir->size = GetMountSize();
        vdir->flags = VFLAG_DIR|base_vdir;
    } else { // non root dir
        if (!(ventry->flags & VFLAG_DIR) || !(ventry->flags & VFLAG_GAMEDIR))
            return false;
        vdir->offset = ventry->offset;
        vdir->size = ventry->size;
        vdir->flags = ventry->flags;
    }
    
    // build directories where required
    if ((vdir->flags & VFLAG_FIRM) && (offset_firm != vdir->offset)) {
        if ((ReadImageBytes((u8*) firm, 0, sizeof(FirmHeader)) != 0) ||
            (ValidateFirmHeader(firm, 0) != 0)) return false;
        offset_firm = vdir->offset;
        FirmSectionHeader* arm9s = FindFirmArm9Section(firm);
        if (arm9s && (ReadImageBytes((u8*) a9l, arm9s->offset, sizeof(FirmA9LHeader)) == 0) &&
            (ValidateFirmA9LHeader(a9l) == 0) &&
            ((SetupArm9BinaryCrypto(a9l)) == 0))
            offset_a9bin = arm9s->offset + ARM9BIN_OFFSET;
        if (!BuildVGameFirmDir()) return false;
    } if ((vdir->flags & VFLAG_CIA) && (offset_cia != vdir->offset)) {
        CiaInfo info;
        if ((ReadImageBytes((u8*) cia, 0, 0x20) != 0) ||
            (ValidateCiaHeader(&(cia->header)) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0) ||
            (ReadImageBytes((u8*) cia, 0, info.offset_content) != 0))
            return false;
        offset_cia = vdir->offset; // always zero(!)
        if (!BuildVGameCiaDir()) return false;
    } else if ((vdir->flags & VFLAG_NCSD) && (offset_ncsd != vdir->offset)) {
        if ((ReadImageBytes((u8*) ncsd, 0, sizeof(NcsdHeader)) != 0) ||
            (ValidateNcsdHeader(ncsd) != 0))
            return false;
        offset_ncsd = vdir->offset; // always zero(!)
        if (!BuildVGameNcsdDir()) return false;
    } else if ((vdir->flags & VFLAG_NCCH) && (offset_ncch != vdir->offset)) {
        offset_ncch = (u64) -1;
        if ((ReadNcchImageBytes((u8*) ncch, vdir->offset, sizeof(NcchHeader)) != 0) ||
            (ValidateNcchHeader(ncch) != 0))
            return false;
        offset_ncch = vdir->offset;
        if (!BuildVGameNcchDir()) return false;
        if (ncch->size_exefs) {
            u32 ncch_offset_exefs = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
            if ((ReadNcchImageBytes((u8*) exefs, ncch_offset_exefs, sizeof(ExeFsHeader)) != 0) ||
                (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
                return false;
            offset_exefs = ncch_offset_exefs;
            if (!BuildVGameExeFsDir()) return false;
        }
    } else if ((vdir->flags & VFLAG_EXEFS) && (offset_exefs != vdir->offset)) {
        if ((ReadNcchImageBytes((u8*) exefs, vdir->offset, sizeof(ExeFsHeader)) != 0) ||
            (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
            return false;
        offset_exefs = vdir->offset;
        if (!BuildVGameExeFsDir()) return false;
    } else if ((vdir->flags & VFLAG_ROMFS) && (offset_romfs != vdir->offset)) {
        // validate romFS magic
        u8 magic[] = { ROMFS_MAGIC };
        u8 header[sizeof(magic)];
        if ((ReadNcchImageBytes(header, vdir->offset, sizeof(magic)) != 0) ||
            (memcmp(magic, header, sizeof(magic)) != 0))
            return false;
        // validate lv3 header
        RomFsLv3Header* lv3 = (RomFsLv3Header*) romfslv3;
        for (u32 i = 1; i < 8; i++) {
            offset_lv3 = vdir->offset + (i*OFFSET_LV3);
            if (ReadNcchImageBytes(romfslv3, offset_lv3, sizeof(RomFsLv3Header)) != 0)
                return false;
            if (ValidateLv3Header(lv3, VGAME_BUFFER_SIZE - 0x20000) == 0)
                break;
            offset_lv3 = (u64) -1;
        }
        if ((offset_lv3 == (u64) -1) || (ReadNcchImageBytes(romfslv3, offset_lv3, lv3->offset_filedata) != 0))
            return false;
        offset_lv3fd = offset_lv3 + lv3->offset_filedata;
        offset_romfs = vdir->offset;
        BuildLv3Index(&lv3idx, romfslv3);
    }
    
    // for romfs dir: switch to lv3 dir object 
    if (vdir->flags & VFLAG_ROMFS) {
        vdir->index = -1;
        vdir->offset = 0;
        vdir->size = 0;
        vdir->flags &= ~VFLAG_ROMFS;
        vdir->flags |= VFLAG_LV3;
    }
    
    return true; // error (should not happen)
}

bool ReadVGameDirLv3(VirtualFile* vfile, VirtualDir* vdir) {
    BuildLv3Index(&lv3idx, romfslv3);
    vfile->flags = VFLAG_LV3;
    vfile->keyslot = ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch)) ? 
        0x2C : 0xFF; // actual keyslot may be different
    
    // start from parent dir object
    if (vdir->index == -1) vdir->index = 0; 
    
    // first child dir object, skip if not available
    if (vdir->index == 0) {
        RomFsLv3DirMeta* parent = LV3_GET_DIR(vdir->offset, &lv3idx);
        if (!parent) return false;
        if (parent->offset_child != (u32) -1) {
            vdir->offset = (u64) parent->offset_child;
            vdir->index = 1;
            vfile->flags |= VFLAG_DIR;
            vfile->offset = vdir->offset;
            return true;
        } else vdir->index = 2;
    }
    
    // parse sibling dirs
    if (vdir->index == 1) {
        RomFsLv3DirMeta* current = LV3_GET_DIR(vdir->offset, &lv3idx);
        if (!current) return false;
        if (current->offset_sibling != (u32) -1) {
            vdir->offset = (u64) current->offset_sibling;
            vfile->flags |= VFLAG_DIR;
            vfile->offset = vdir->offset;
            return true;
        } else if (current->offset_parent != (u32) -1) {
            vdir->offset = (u64) current->offset_parent;
            vdir->index = 2;
        } else return false;
    }
    
    // first child file object, skip if not available
    if (vdir->index == 2) {
        RomFsLv3DirMeta* parent = LV3_GET_DIR(vdir->offset, &lv3idx);
        if (!parent) return false;
        if (parent->offset_file != (u32) -1) {
            vdir->offset = (u64) parent->offset_file;
            vdir->index = 3;
            RomFsLv3FileMeta* lv3file = LV3_GET_FILE(vdir->offset, &lv3idx);
            if (!lv3file) return false;
            vfile->offset = vdir->offset;
            vfile->size = lv3file->size_data;
            return true;
        } else vdir->index = 4;
    }
    
    // parse sibling files
    if (vdir->index == 3) {
        RomFsLv3FileMeta* current = LV3_GET_FILE(vdir->offset, &lv3idx);
        if (!current) return false;
        if (current->offset_sibling != (u32) -1) {
            vdir->offset = current->offset_sibling;
            RomFsLv3FileMeta* lv3file = LV3_GET_FILE(vdir->offset, &lv3idx);
            if (!lv3file) return false;
            vfile->offset = vdir->offset;
            vfile->size = lv3file->size_data;
            return true;
        } else if (current->offset_parent != (u32) -1) {
            vdir->offset = current->offset_parent;
            vdir->index = 4;
        } else return false;
    }
    
    return false;
}

bool ReadVGameDir(VirtualFile* vfile, VirtualDir* vdir) {
    VirtualFile* templates;
    int n = 0;
    
    if (vdir->flags & VFLAG_FIRM) {
        templates = templates_firm;
        n = n_templates_firm;
    } else if (vdir->flags & VFLAG_CIA) {
        templates = templates_cia;
        n = n_templates_cia;
    } else if (vdir->flags & VFLAG_NCSD) {
        templates = templates_ncsd;
        n = n_templates_ncsd;
    } else if (vdir->flags & VFLAG_NCCH) {
        templates = templates_ncch;
        n = n_templates_ncch;
    } else if (vdir->flags & VFLAG_EXEFS) {
        templates = templates_exefs;
        n = n_templates_exefs;
    } else if (vdir->flags & VFLAG_LV3) {
        return ReadVGameDirLv3(vfile, vdir);
    }
    
    if (++vdir->index < n) {
        // copy current template to vfile
        memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
        return true;
    }
    
    return false;
}

int ReadVGameFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count) {
    u32 vfoffset = vfile->offset;
    if (vfile->flags & VFLAG_LV3) {
        RomFsLv3FileMeta* lv3file;
        if (vfile->flags & VFLAG_DIR) return -1;
        lv3file = LV3_GET_FILE(vfile->offset, &lv3idx);
        vfoffset = offset_lv3fd + lv3file->offset_data;
    }
    if (vfile->flags & (VFLAG_EXEFS_FILE|VFLAG_EXTHDR|VFLAG_EXEFS|VFLAG_ROMFS|VFLAG_LV3|VFLAG_NCCH))
        return ReadNcchImageBytes(buffer, vfoffset + offset, count);
    else if ((offset_a9bin != (u64) -1) && !(vfile->flags & VFLAG_FIRM_SECTION))
        return ReadFirmImageBytes(buffer, vfoffset + offset, count);
    else return ReadImageBytes(buffer, vfoffset + offset, count);
}

bool FindVirtualFileInLv3Dir(VirtualFile* vfile, const VirtualDir* vdir, const char* name) {
    vfile->name[0] = '\0';
    vfile->flags = vdir->flags & ~VFLAG_DIR;
    vfile->keyslot = ((offset_ncch != (u64) -1) && NCCH_ENCRYPTED(ncch)) ?
        0x2C : 0xFF; // actual keyslot may be different
    
    RomFsLv3DirMeta* lv3dir = GetLv3DirMeta(name, vdir->offset, &lv3idx);
    if (lv3dir) {
        vfile->offset = ((u8*) lv3dir) - ((u8*) lv3idx.dirmeta);
        vfile->size = 0;
        vfile->flags |= VFLAG_DIR;
        return true;
    }
    
    RomFsLv3FileMeta* lv3file = GetLv3FileMeta(name, vdir->offset, &lv3idx);
    if (lv3file) {
        vfile->offset = ((u8*) lv3file) - ((u8*) lv3idx.filemeta);
        vfile->size = lv3file->size_data;
        return true;
    }
    
    return false;
}

bool GetVGameLv3Filename(char* name, const VirtualFile* vfile, u32 n_chars) {
    if (!(vfile->flags & VFLAG_LV3))
        return false;
    
    u16* wname = NULL;
    u32 name_len = 0;
    
    if (vfile->flags & VFLAG_DIR) {
        RomFsLv3DirMeta* dirmeta = LV3_GET_DIR(vfile->offset, &lv3idx);
        if (!dirmeta) return false;
        wname = dirmeta->wname;
        name_len = dirmeta->name_len / 2;
    } else {
        RomFsLv3FileMeta* filemeta = LV3_GET_FILE(vfile->offset, &lv3idx);
        if (!filemeta) return false;
        wname = filemeta->wname;
        name_len = filemeta->name_len / 2;
    }
    memset(name, 0, n_chars);
    for (u32 i = 0; (i < (n_chars-1)) && (i < name_len); i++)
        name[i] = wname[i]; // poor mans UTF-16 -> UTF-8
    
    return true;
}

bool MatchVGameLv3Filename(const char* name, const VirtualFile* vfile, u32 n_chars) {
    char lv3_name[256];
    if (!GetVGameLv3Filename(lv3_name, vfile, 256)) return false;
    return (strncasecmp(name, lv3_name, n_chars) == 0);
}

u64 GetVGameDriveSize(void) {
    return (vgame_type) ? GetMountSize() : 0;
}
