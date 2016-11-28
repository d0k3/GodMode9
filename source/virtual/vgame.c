#include "vgame.h"
#include "image.h"
#include "game.h"
#include "aes.h"
#include "ff.h"

#define VFLAG_EXTHDR        (1<<29)
#define VFLAG_EXEFS         (1<<30)
#define VFLAG_ROMFS         (1<<31)

#define MAX_N_TEMPLATES 2048 // this leaves us with enough room (128kB reserved)

#define NAME_CIA_HEADER     "header.bin"
#define NAME_CIA_CERT       "cert.bin"
#define NAME_CIA_TICKET     "ticket.bin"
#define NAME_CIA_TMD        "tmd.bin"
#define NAME_CIA_TMDCHUNK   "tmdchunks.bin"
#define NAME_CIA_META       "meta.bin"
#define NAME_CIA_CONTENT    "%04X.%08lX.app" // index.id.app

#define NAME_NCSD_HEADER    "ncsd.bin"
#define NAME_NCSD_CARDINFO  "cardinfo.bin"
#define NAME_NCSD_DEVINFO   "devinfo.bin"
#define NAME_NCSD_CONTENT   "cnt0.game.cxi", "cnt1.manual.cfa", "cnt2.dlp.cfa", \
                            "cnt3.unk", "cnt4.unk", "cnt5.unk", \
                            "cnt6.update_n3ds.cfa", "cnt7.update_o3ds.cfa"

#define NAME_NCCH_HEADER    "ncch.bin"
#define NAME_NCCH_EXTHEADER "extheader.bin"
#define NAME_NCCH_PLAIN     "plain.bin"
#define NAME_NCCH_LOGO      "logo.bin"
#define NAME_NCCH_EXEFS     "exefs.bin"
#define NAME_NCCH_ROMFS     "romfs.bin"

static u32 vgame_type = 0;
static VirtualFile* templates = (VirtualFile*) VGAME_BUFFER; // first 128kb reserved
static int n_templates = -1;

static CiaStub* cia = (CiaStub*) (VGAME_BUFFER + 0xF4000); // 48kB reserved - should be enough by far
static NcsdHeader* ncsd = (NcsdHeader*) (VGAME_BUFFER + 0xF3000); // needs only 512 byte
static NcchHeader* ncch = (NcchHeader*) (VGAME_BUFFER + 0xF3200); // needs only 512 byte
static ExeFsHeader* exefs = (ExeFsHeader*) (VGAME_BUFFER + 0xF3400); // needs only 512 byte

static u32 offset_ncch = 0;
static u32 offset_exefs = 0;
static u32 offset_romfs = 0;

u32 InitVGameDrive(void) { // prerequisite: game file mounted as image
    u32 type = GetMountState();
    vgame_type = 0;
    if (type == GAME_CIA) { // for CIAs: load the CIA stub and keep it in memory
        CiaInfo info;
        if ((ReadImageBytes((u8*) cia, 0, 0x20) != 0) ||
            (ValidateCiaHeader(&(cia->header)) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0) ||
            (ReadImageBytes((u8*) cia, 0, info.offset_content) != 0))
            return 0;
    } else if (type == GAME_NCSD) {
        if ((ReadImageBytes((u8*) ncsd, 0, sizeof(NcsdHeader)) != 0) ||
            (ValidateNcsdHeader(ncsd) != 0))
            return 0;
    } else if (type == GAME_NCCH) {
        offset_ncch = 0;
        if ((ReadImageBytes((u8*) ncch, 0, sizeof(NcchHeader)) != 0) ||
            (ValidateNcchHeader(ncch) != 0))
            return 0;
    } else return 0; // not a mounted game file
    
    vgame_type = type;
    return type;
}

u32 CheckVGameDrive(void) {
    if (vgame_type != GetMountState()) vgame_type = 0; // very basic sanity check
    return vgame_type;
}

bool BuildVGameNcchVDir(void) {
    if (CheckVGameDrive() != GAME_NCCH)
        return false; // safety check
    
    // header
    strncpy(templates[n_templates].name, NAME_NCCH_HEADER, 32);
    templates[n_templates].offset = offset_ncch + 0;
    templates[n_templates].size = 0x200;
    templates[n_templates].keyslot = 0xFF;
    templates[n_templates].flags = 0;
    n_templates++;
    
    // extended header
    if (ncch->size_exthdr) {
        strncpy(templates[n_templates].name, NAME_NCCH_EXTHEADER, 32);
        templates[n_templates].offset = offset_ncch + NCCH_EXTHDR_OFFSET;
        templates[n_templates].size = NCCH_EXTHDR_SIZE;
        templates[n_templates].keyslot = 0xFF; // crypto ?
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // plain region
    if (ncch->size_plain) {
        strncpy(templates[n_templates].name, NAME_NCCH_PLAIN, 32);
        templates[n_templates].offset = offset_ncch + (ncch->offset_plain * NCCH_MEDIA_UNIT);
        templates[n_templates].size = ncch->size_plain * NCCH_MEDIA_UNIT;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // logo region
    if (ncch->size_logo) {
        strncpy(templates[n_templates].name, NAME_NCCH_LOGO, 32);
        templates[n_templates].offset =offset_ncch + (ncch->offset_logo * NCCH_MEDIA_UNIT);
        templates[n_templates].size = ncch->size_logo * NCCH_MEDIA_UNIT;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // exefs
    if (ncch->size_exefs) {
        strncpy(templates[n_templates].name, NAME_NCCH_EXEFS, 32);
        templates[n_templates].offset = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
        templates[n_templates].size = ncch->size_exefs * NCCH_MEDIA_UNIT;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // romfs
    if (ncch->size_romfs) {
        strncpy(templates[n_templates].name, NAME_NCCH_ROMFS, 32);
        templates[n_templates].offset = offset_ncch + (ncch->offset_romfs * NCCH_MEDIA_UNIT);
        templates[n_templates].size = ncch->size_romfs * NCCH_MEDIA_UNIT;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    return 0;
}
    
bool BuildVGameNcsdVDir(void) {
    const char* name_content[] = { NAME_NCSD_CONTENT };
    
    if (CheckVGameDrive() != GAME_NCSD)
        return false; // safety check
    
    // header
    strncpy(templates[n_templates].name, NAME_NCSD_HEADER, 32);
    templates[n_templates].offset = 0;
    templates[n_templates].size = 0x200;
    templates[n_templates].keyslot = 0xFF;
    templates[n_templates].flags = 0;
    n_templates++;
    
    // card info header
    if (ncsd->partitions[0].offset * NCSD_MEDIA_UNIT >= NCSD_CINFO_OFFSET + NCSD_CINFO_SIZE) {
        strncpy(templates[n_templates].name, NAME_NCSD_CARDINFO, 32);
        templates[n_templates].offset = NCSD_CINFO_OFFSET;
        templates[n_templates].size = NCSD_CINFO_SIZE;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // dev info header
    if (ncsd->partitions[0].offset * NCSD_MEDIA_UNIT >= NCSD_DINFO_OFFSET + NCSD_DINFO_SIZE) {
        strncpy(templates[n_templates].name, NAME_NCSD_DEVINFO, 32);
        templates[n_templates].offset = NCSD_DINFO_OFFSET;
        templates[n_templates].size = NCSD_DINFO_SIZE;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // contents
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd->partitions + i;
        if ((partition->offset == 0) && (partition->size == 0))
            continue;
        strncpy(templates[n_templates].name, name_content[i], 32);
        templates[n_templates].offset = partition->offset * NCSD_MEDIA_UNIT;
        templates[n_templates].size = partition->size * NCSD_MEDIA_UNIT;
        templates[n_templates].keyslot = 0xFF; // even for encrypted stuff
        templates[n_templates].flags = 0; // this handles encryption
        n_templates++;
    }
    
    return true;
}
    
bool BuildVGameCiaVDir(void) {
    CiaInfo info;
    
    if ((CheckVGameDrive() != GAME_CIA) || (GetCiaInfo(&info, &(cia->header)) != 0))
        return false; // safety check
    
    // header
    strncpy(templates[n_templates].name, NAME_CIA_HEADER, 32);
    templates[n_templates].offset = 0;
    templates[n_templates].size = info.size_header;
    templates[n_templates].keyslot = 0xFF;
    templates[n_templates].flags = 0;
    n_templates++;
    
    // certificates
    if (info.size_cert) {
        strncpy(templates[n_templates].name, NAME_CIA_CERT, 32);
        templates[n_templates].offset = info.offset_cert;
        templates[n_templates].size = info.size_cert;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // ticket
    if (info.size_ticket) {
        strncpy(templates[n_templates].name, NAME_CIA_TICKET, 32);
        templates[n_templates].offset = info.offset_ticket;
        templates[n_templates].size = info.size_ticket;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // TMD (the full thing)
    if (info.size_tmd) {
        strncpy(templates[n_templates].name, NAME_CIA_TMD, 32);
        templates[n_templates].offset = info.offset_tmd;
        templates[n_templates].size = info.size_tmd;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // TMD content chunks
    if (info.size_content_list) {
        strncpy(templates[n_templates].name, NAME_CIA_TMDCHUNK, 32);
        templates[n_templates].offset = info.offset_content_list;
        templates[n_templates].size = info.size_content_list;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // meta
    if (info.size_meta) {
        strncpy(templates[n_templates].name, NAME_CIA_META, 32);
        templates[n_templates].offset = info.offset_meta;
        templates[n_templates].size = info.size_meta;
        templates[n_templates].keyslot = 0xFF;
        templates[n_templates].flags = 0;
        n_templates++;
    }
    
    // contents
    if (info.size_content) {
        TmdContentChunk* content_list = cia->content_list;
        u32 content_count = getbe16(cia->tmd.content_count);
        u64 next_offset = info.offset_content;
        for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
            u64 size = getbe64(content_list[i].size);
            // bool encrypted = getbe16(content_list[i].type) & 0x1;
            snprintf(templates[n_templates].name, 32, NAME_CIA_CONTENT,
                getbe16(content_list[i].index), getbe32(content_list[i].id));
            templates[n_templates].offset = (u32) next_offset;
            templates[n_templates].size = (u32) size;
            templates[n_templates].keyslot = 0xFF; // even for encrypted stuff
            templates[n_templates].flags = 0; // this handles encryption
            n_templates++;
            next_offset += size;
        }
    }
    
    return true;
}

bool ReadVGameDir(VirtualFile* vfile, const char* path) {
    
    (void) path; // not in use yet
    static int num = -1;
    
    if (!vfile) { // NULL pointer 
        num = -1; // reset dir reader / internal number
        memset(templates, 0, sizeof(VirtualFile) * MAX_N_TEMPLATES);
        n_templates = 0;
        if ((vgame_type == GAME_CIA) && BuildVGameCiaVDir()) // for CIA
            return true;
        else if ((vgame_type == GAME_NCSD) && BuildVGameNcsdVDir()) // for NCSD
            return true;
        else if ((vgame_type == GAME_NCCH) && BuildVGameNcchVDir()) // for NCSD
            return true;
        return false;
    }
    
    if (++num < n_templates) {
        // copy current template to vfile
        memcpy(vfile, templates + num, sizeof(VirtualFile));
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
