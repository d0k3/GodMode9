#include "vgame.h"
#include "image.h"
#include "game.h"
#include "aes.h"
#include "ff.h"

#define VFLAG_EXTHDR        (1<<26)
#define VFLAG_CIA           (1<<27) // unused, see below
#define VFLAG_NCSD          (1<<28) // unused, see below
#define VFLAG_NCCH          (1<<29)
#define VFLAG_EXEFS         (1<<30)
#define VFLAG_ROMFS         (1<<31)

#define VDIR_CIA            VFLAG_CIA
#define VDIR_NCSD           VFLAG_NCSD
#define VDIR_NCCH           VFLAG_NCCH
#define VDIR_EXEFS          VFLAG_EXEFS
#define VDIR_ROMFS          VFLAG_ROMFS
#define VDIR_GAME           (VDIR_CIA|VDIR_NCSD|VDIR_NCCH|VDIR_EXEFS|VDIR_ROMFS)

#define MAX_N_TEMPLATES 2048 // this leaves us with enough room (128kB reserved)

#define NAME_CIA_HEADER     "header.bin"
#define NAME_CIA_CERT       "cert.bin"
#define NAME_CIA_TICKET     "ticket.bin"
#define NAME_CIA_TMD        "tmd.bin"
#define NAME_CIA_TMDCHUNK   "tmdchunks.bin"
#define NAME_CIA_META       "meta.bin"
#define NAME_CIA_CONTENT    "%04X.%08lX.app" // index.id.app
#define NAME_CIA_DIR        "%04X.%08lX" // index.id

#define NAME_NCSD_HEADER    "ncsd.bin"
#define NAME_NCSD_CARDINFO  "cardinfo.bin"
#define NAME_NCSD_DEVINFO   "devinfo.bin"
#define NAME_NCSD_CONTENT   "cnt0.game.cxi", "cnt1.manual.cfa", "cnt2.dlp.cfa", \
                            "cnt3.unk", "cnt4.unk", "cnt5.unk", \
                            "cnt6.update_n3ds.cfa", "cnt7.update_o3ds.cfa"
#define NAME_NCSD_DIR       "cnt0.game", "cnt1.manual", "cnt2.dlp", \
                            "cnt3", "cnt4", "cnt5", \
                            "cnt6.update_n3ds", "cnt7.update_o3ds"

#define NAME_NCCH_HEADER    "ncch.bin"
#define NAME_NCCH_EXTHEADER "extheader.bin"
#define NAME_NCCH_PLAIN     "plain.bin"
#define NAME_NCCH_LOGO      "logo.bin"
#define NAME_NCCH_EXEFS     "exefs.bin"
#define NAME_NCCH_ROMFS     "romfs.bin"

static u32 vgame_type = 0;
static u32 base_vdir = 0;

static VirtualFile* templates_cia   = (VirtualFile*) VGAME_BUFFER; // first 116kb reserved (enough for ~2000 entries)
static VirtualFile* templates_ncsd  = (VirtualFile*) VGAME_BUFFER + 0x1D000; // 4kb reserved (enough for ~80 entries)
static VirtualFile* templates_ncch  = (VirtualFile*) VGAME_BUFFER + 0x1E000; // 4kb reserved (enough for ~80 entries)
static VirtualFile* templates_exefs = (VirtualFile*) VGAME_BUFFER + 0x1F000; // 4kb reserved (enough for ~80 entries)
static int n_templates_cia   = -1;
static int n_templates_ncsd  = -1;
static int n_templates_ncch  = -1;
static int n_templates_exefs = -1;

static u32 offset_ncch = 0;
static u32 offset_exefs = 0;
static u32 offset_romfs = 0;

static CiaStub* cia = (CiaStub*) (VGAME_BUFFER + 0xF4000); // 48kB reserved - should be enough by far
static NcsdHeader* ncsd = (NcsdHeader*) (VGAME_BUFFER + 0xF3000); // 512 byte reserved
static NcchHeader* ncch = (NcchHeader*) (VGAME_BUFFER + 0xF3200); // 512 byte reserved
static ExeFsHeader* exefs = (ExeFsHeader*) (VGAME_BUFFER + 0xF3400); // 512 byte reserved

bool BuildVGameNcchDir(void) {
    VirtualFile* templates = templates_ncch;
    u32 n = 0;
    
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
        templates[n].keyslot = 0xFF; // crypto ?
        templates[n].flags = 0;
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
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    // romfs
    if (ncch->size_romfs) {
        strncpy(templates[n].name, NAME_NCCH_ROMFS, 32);
        templates[n].offset = offset_ncch + (ncch->offset_romfs * NCCH_MEDIA_UNIT);
        templates[n].size = ncch->size_romfs * NCCH_MEDIA_UNIT;
        templates[n].keyslot = 0xFF;
        templates[n].flags = 0;
        n++;
    }
    
    n_templates_ncch = n;
    return true;
}
    
bool BuildVGameNcsdDir(void) {
    const char* name_content[] = { NAME_NCSD_CONTENT };
    const char* name_dir[] = { NAME_NCSD_DIR };
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
        strncpy(templates[n].name, name_content[i], 32);
        templates[n].offset = partition->offset * NCSD_MEDIA_UNIT;
        templates[n].size = partition->size * NCSD_MEDIA_UNIT;
        templates[n].keyslot = 0xFF; // not encrypted
        templates[n].flags = VFLAG_NCCH;
        n++;
        memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
        strncpy(templates[n].name, name_dir[i], 32);
        templates[n].flags |= VFLAG_DIR;
        n++;
    }
    
    n_templates_ncsd = n;
    return true;
}
#include "ui.h"
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
        for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
            u64 size = getbe64(content_list[i].size);
            bool is_ncch = false; // is unencrypted NCCH?
            if (!(getbe16(content_list[i].type) & 0x1)) {
                NcchHeader ncch;
                ReadImageBytes((u8*) &ncch, next_offset, sizeof(NcchHeader));
                is_ncch = (ValidateNcchHeader(&ncch) == 0);
            }
            snprintf(templates[n].name, 32, NAME_CIA_CONTENT,
                getbe16(content_list[i].index), getbe32(content_list[i].id));
            templates[n].offset = next_offset;
            templates[n].size = size;
            templates[n].keyslot = 0xFF; // even for encrypted stuff
            templates[n].flags = is_ncch ? VFLAG_NCCH : 0;
            n++;
            if (is_ncch) {
                memcpy(templates + n, templates + n - 1, sizeof(VirtualFile));
                snprintf(templates[n].name, 32, NAME_CIA_DIR,
                    getbe16(content_list[i].index), getbe32(content_list[i].id));
                templates[n].flags |= VFLAG_DIR;
                n++;
            }
            next_offset += size;
        }
    }
    
    n_templates_cia = n;
    return true;
}

u32 InitVGameDrive(void) { // prerequisite: game file mounted as image
    u32 type = GetMountState();
    vgame_type = 0;
    offset_ncch = 0;
    offset_exefs = 0;
    offset_romfs = 0;
    if (type == GAME_CIA) { // for CIAs: load the CIA stub and keep it in memory
        CiaInfo info;
        if ((ReadImageBytes((u8*) cia, 0, 0x20) != 0) ||
            (ValidateCiaHeader(&(cia->header)) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0) ||
            (ReadImageBytes((u8*) cia, 0, info.offset_content) != 0))
            return 0;
        if (!BuildVGameCiaDir()) return 0;
        base_vdir = VDIR_CIA;
    } else if (type == GAME_NCSD) {
        if ((ReadImageBytes((u8*) ncsd, 0, sizeof(NcsdHeader)) != 0) ||
            (ValidateNcsdHeader(ncsd) != 0))
            return 0;
        if (!BuildVGameNcsdDir()) return 0;
        base_vdir = VDIR_NCSD;
    } else if (type == GAME_NCCH) {
        if ((ReadImageBytes((u8*) ncch, 0, sizeof(NcchHeader)) != 0) ||
            (ValidateNcchHeader(ncch) != 0))
            return 0;
        if (!BuildVGameNcchDir()) return 0;
        base_vdir = VDIR_NCCH;
        offset_ncch = 0;
        offset_exefs = ncch->offset_exefs * NCCH_MEDIA_UNIT;
        offset_romfs = ncch->offset_romfs * NCCH_MEDIA_UNIT;
    } else return 0; // not a mounted game file
    
    vgame_type = type;
    return type;
}

u32 CheckVGameDrive(void) {
    if (vgame_type != GetMountState()) vgame_type = 0; // very basic sanity check
    return vgame_type;
}

bool OpenVGameDir(VirtualDir* vdir, VirtualFile* ventry) {
    vdir->index = -1;
    vdir->virtual_src = VRT_GAME;
    if (!ventry) { // root dir
        vdir->offset = 0;
        vdir->size = GetMountSize();
        vdir->flags = VFLAG_DIR|base_vdir;
        // base dir is already in memory -> done
    } else { // non root dir
        if (!(ventry->flags & VFLAG_DIR) || !(ventry->flags & VDIR_GAME))
            return false;
        vdir->offset = ventry->offset;
        vdir->size = ventry->size;
        vdir->flags = ventry->flags;
        // build directories where required
        u32 curr_vdir = vdir->flags & VDIR_GAME;
        if ((curr_vdir == VDIR_NCCH) && (offset_ncch != vdir->offset)) {
            if ((ReadImageBytes((u8*) ncch, vdir->offset, sizeof(NcchHeader)) != 0) ||
                (ValidateNcchHeader(ncch) != 0))
                return false;
            offset_ncch = vdir->offset;
            if (!BuildVGameNcchDir()) return false;
        } else if ((curr_vdir == VDIR_EXEFS) && (offset_exefs != vdir->offset)) {
            if ((ReadImageBytes((u8*) exefs, vdir->offset, sizeof(ExeFsHeader)) != 0) ||
                (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
                return false;
            offset_exefs = vdir->offset;
        }
    }
    
    return true; // error (should not happen)
}

bool ReadVGameDir(VirtualFile* vfile, VirtualDir* vdir) {
    u32 curr_vdir = vdir->flags & VDIR_GAME;
    VirtualFile* templates;
    int n = 0;
    
    if (curr_vdir == VDIR_CIA) {
        templates = templates_cia;
        n = n_templates_cia;
    } else if (curr_vdir == VDIR_NCSD) {
        templates = templates_ncsd;
        n = n_templates_ncsd;
    } else if (curr_vdir == VDIR_NCCH) {
        templates = templates_ncch;
        n = n_templates_ncch;
    } else if (curr_vdir == VDIR_EXEFS) {
        templates = templates_exefs;
        n = n_templates_exefs;
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
    int ret = ReadImageBytes(buffer, vfoffset + offset, count);
    if (ret != 0) return ret;
    /*if ((ret != 0) && (vfile->keyslot <= 0x40)) { // crypto
        // relies on first template being the header and everything aligned to AES_BLOCK_SIZE
        u32 offset_base = 0; // vfoffset - (*templates).offset;
        u8 ctr[16] = { 0 };
        ctr[0] = (vfile->index & 0xFF);
        ctr[1] = (vfile->index >> 8);
        setup_aeskeyY(0x11, titlekey);
        use_aeskey(0x11);
        ctr_decrypt_boffset(buffer, buffer, bytes_read, offset - offset_base,
            AES_CNT_TITLEKEY_DECRYPT_MODE, ctr);
    }*/
    return 0;
}
