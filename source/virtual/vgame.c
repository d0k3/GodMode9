#include "vgame.h"
#include "image.h"
#include "game.h"
#include "aes.h"
#include "ff.h"

#define VFLAG_EXTHDR        (1<<25)
#define VFLAG_CIA           (1<<26) // unused, see below
#define VFLAG_NCSD          (1<<27) // unused, see below
#define VFLAG_NCCH          (1<<28)
#define VFLAG_EXEFS         (1<<29)
#define VFLAG_ROMFS         (1<<30)
#define VFLAG_LV3           (1<<31)

#define VDIR_CIA            VFLAG_CIA
#define VDIR_NCSD           VFLAG_NCSD
#define VDIR_NCCH           VFLAG_NCCH
#define VDIR_EXEFS          VFLAG_EXEFS
#define VDIR_ROMFS          VFLAG_ROMFS
#define VDIR_LV3            VFLAG_LV3
#define VDIR_GAME           (VDIR_CIA|VDIR_NCSD|VDIR_NCCH|VDIR_EXEFS|VDIR_ROMFS|VDIR_LV3)

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
#define NAME_NCCH_EXEFSDIR  "exefs"
#define NAME_NCCH_ROMFSDIR  "romfs"


static u32 vgame_type = 0;
static u32 base_vdir = 0;

static VirtualFile* templates_cia   = (VirtualFile*) VGAME_BUFFER; // first 52kb reserved (enough for 950 entries)
static VirtualFile* templates_ncsd  = (VirtualFile*) VGAME_BUFFER + 0xE800; // 2kb reserved (enough for 36 entries)
static VirtualFile* templates_ncch  = (VirtualFile*) VGAME_BUFFER + 0xF000; // 2kb reserved (enough for 36 entries)
static VirtualFile* templates_exefs = (VirtualFile*) VGAME_BUFFER + 0xF800; // 2kb reserved (enough for 36 entries)
static int n_templates_cia   = -1;
static int n_templates_ncsd  = -1;
static int n_templates_ncch  = -1;
static int n_templates_exefs = -1;

static u64 offset_cia   = (u64) -1;
static u64 offset_ncsd  = (u64) -1;
static u64 offset_ncch  = (u64) -1;
static u64 offset_exefs = (u64) -1;
static u64 offset_romfs = (u64) -1;
static u64 offset_lv3   = (u64) -1;
static u64 offset_lv3fd = (u64) -1;

static CiaStub* cia = (CiaStub*) (VGAME_BUFFER + 0x10000); // 62.5kB reserved - should be enough by far
static NcsdHeader* ncsd = (NcsdHeader*) (VGAME_BUFFER + 0x1FA00); // 512 byte reserved
static NcchHeader* ncch = (NcchHeader*) (VGAME_BUFFER + 0x1FC00); // 512 byte reserved
static ExeFsHeader* exefs = (ExeFsHeader*) (VGAME_BUFFER + 0x1FE00); // 512 byte reserved
static u8* romfslv3 = (u8*) (VGAME_BUFFER + 0x20000); // 1920kB reserved
static RomFsLv3Index lv3idx;

bool BuildVGameExeFsDir(void) {
    VirtualFile* templates = templates_exefs;
    u32 n = 0;
    
    for (u32 i = 0; i < 10; i++) {
        ExeFsFileHeader* file = exefs->files + i;
        if (file->size == 0) continue;
        snprintf(templates[n].name, 32, "%.8s", file->name);
        templates[n].offset = offset_exefs + sizeof(ExeFsHeader) + file->offset;
        templates[n].size = file->size;
        templates[n].keyslot = 0xFF; // needs to be handled
        templates[n].flags = 0;
        n++;
    }
    
    n_templates_exefs = n;
    return true;
}

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
        templates[n].keyslot = 0xFF; // crypto ?
        templates[n].flags = VFLAG_EXEFS;
        n++;
        if (!NCCH_ENCRYPTED(ncch)) {
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
        templates[n].keyslot = 0xFF; // crypto ?
        templates[n].flags = VFLAG_ROMFS;
        n++;
        if (!NCCH_ENCRYPTED(ncch)) {
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
    offset_cia   = (u64) -1;
    offset_ncsd  = (u64) -1;
    offset_ncch  = (u64) -1;
    offset_exefs = (u64) -1;
    offset_romfs = (u64) -1;
    offset_lv3   = (u64) -1;
    offset_lv3fd = (u64) -1;
    
    base_vdir = (type == GAME_CIA) ? VDIR_CIA : (type == GAME_NCSD) ? VDIR_NCSD : (type == GAME_NCCH) ? VDIR_NCCH : 0;
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
    vdir->virtual_src = VRT_GAME;
    if (!ventry) { // root dir
        vdir->offset = 0;
        vdir->size = GetMountSize();
        vdir->flags = VFLAG_DIR|base_vdir;
    } else { // non root dir
        if (!(ventry->flags & VFLAG_DIR) || !(ventry->flags & VDIR_GAME))
            return false;
        vdir->offset = ventry->offset;
        vdir->size = ventry->size;
        vdir->flags = ventry->flags;
    }
    
    // build directories where required
    if ((vdir->flags & VDIR_CIA) && (offset_cia != vdir->offset)) {
        CiaInfo info;
        if ((ReadImageBytes((u8*) cia, 0, 0x20) != 0) ||
            (ValidateCiaHeader(&(cia->header)) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0) ||
            (ReadImageBytes((u8*) cia, 0, info.offset_content) != 0))
            return false;
        offset_cia = vdir->offset; // always zero(!)
        if (!BuildVGameCiaDir()) return false;
    } else if ((vdir->flags & VDIR_NCSD) && (offset_ncsd != vdir->offset)) {
        if ((ReadImageBytes((u8*) ncsd, 0, sizeof(NcsdHeader)) != 0) ||
            (ValidateNcsdHeader(ncsd) != 0))
            return 0;
        offset_ncsd = vdir->offset; // always zero(!)
        if (!BuildVGameNcsdDir()) return 0;
    } else if ((vdir->flags & VDIR_NCCH) && (offset_ncch != vdir->offset)) {
        if ((ReadImageBytes((u8*) ncch, vdir->offset, sizeof(NcchHeader)) != 0) ||
            (ValidateNcchHeader(ncch) != 0))
            return false;
        offset_ncch = vdir->offset;
        if (!BuildVGameNcchDir()) return false;
    } else if ((vdir->flags & VDIR_EXEFS) && (offset_exefs != vdir->offset)) {
        if ((ReadImageBytes((u8*) exefs, vdir->offset, sizeof(ExeFsHeader)) != 0) ||
            (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
            return false;
        offset_exefs = vdir->offset;
        if (!BuildVGameExeFsDir()) return false;
    } else if ((vdir->flags & VDIR_ROMFS) && (offset_romfs != vdir->offset)) {
        // validate romFS magic
        u8 magic[] = { ROMFS_MAGIC };
        u8 header[sizeof(magic)];
        if ((ReadImageBytes(header, vdir->offset, sizeof(magic)) != 0) ||
            (memcmp(magic, header, sizeof(magic)) != 0))
            return false;
        // validate lv3 header
        RomFsLv3Header* lv3 = (RomFsLv3Header*) romfslv3;
        for (u32 i = 1; i < 8; i++) {
            offset_lv3 = vdir->offset + (i*OFFSET_LV3);
            if (ReadImageBytes(romfslv3, offset_lv3, sizeof(RomFsLv3Header)) != 0)
                return false;
            if (ValidateLv3Header(lv3, VGAME_BUFFER_SIZE - 0x20000) == 0)
                break;
            offset_lv3 = (u64) -1;
        }
        if ((offset_lv3 == (u64) -1) || (ReadImageBytes(romfslv3, offset_lv3, lv3->offset_filedata) != 0))
            return false;
        offset_lv3fd = offset_lv3 + lv3->offset_filedata;
        offset_romfs = vdir->offset;
        BuildLv3Index(&lv3idx, romfslv3);
    }
    
    // for romfs dir: switch to lv3 dir object 
    if (vdir->flags & VDIR_ROMFS) {
        vdir->index = -1;
        vdir->offset = 0;
        vdir->size = 0;
        vdir->flags &= ~VDIR_ROMFS;
        vdir->flags |= VDIR_LV3;
    }
    
    return true; // error (should not happen)
}

bool ReadVGameDirLv3(VirtualFile* vfile, VirtualDir* vdir) {
    BuildLv3Index(&lv3idx, romfslv3);
    vfile->flags = VFLAG_LV3;
    
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
    
    if (vdir->flags & VDIR_CIA) {
        templates = templates_cia;
        n = n_templates_cia;
    } else if (vdir->flags & VDIR_NCSD) {
        templates = templates_ncsd;
        n = n_templates_ncsd;
    } else if (vdir->flags & VDIR_NCCH) {
        templates = templates_ncch;
        n = n_templates_ncch;
    } else if (vdir->flags & VDIR_EXEFS) {
        templates = templates_exefs;
        n = n_templates_exefs;
    } else if (vdir->flags & VDIR_LV3) {
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

bool GetVGameFilename(char* name, const VirtualFile* vfile, u32 n_chars) {
    if (!(vfile->flags & VFLAG_LV3)) {
        snprintf(name, n_chars, "%s", vfile->name);
        return true;
    }
    
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

bool MatchVGameFilename(const char* name, const VirtualFile* vfile, u32 n_chars) {
    char vg_name[256];
    if (!GetVGameFilename(vg_name, vfile, 256)) return false;
    return (strncasecmp(name, vg_name, n_chars) == 0);
}
