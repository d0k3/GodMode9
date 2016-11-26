#include "vgame.h"
#include "image.h"
#include "game.h"
#include "aes.h"
#include "ff.h"

#define MAX_N_TEMPLATES 2048 // this leaves us with enough room (128kB reserved)

#define NAME_CIA_HEADER     "header.bin"
#define NAME_CIA_CERT       "cert.bin"
#define NAME_CIA_TICKET     "ticket.bin"
#define NAME_CIA_TMD        "tmd.bin"
#define NAME_CIA_TMDCHUNK   "tmdchunks.bin"
#define NAME_CIA_META       "meta.bin"
#define NAME_CIA_CONTENT    "%04X.%08lX.app" // index.id.app

static u32 vgame_type = 0;
static VirtualFile* templates = (VirtualFile*) VGAME_BUFFER; // first 128kb reserved
static int n_templates = -1;

static CiaStub* cia = (CiaStub*) (VGAME_BUFFER + 0xF4000); // 48kB reserved - should be enough by far

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
    } else if ((type == GAME_NCCH) || (type == GAME_NCSD)) {
    } else return 0; // not a mounted game file
    
    vgame_type = type;
    return type;
}

u32 CheckVGameDrive(void) {
    if (vgame_type != GetMountState()) vgame_type = 0; // very basic sanity check
    return vgame_type;
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
        if (!BuildVGameCiaVDir()) // NCCH / NCSD !!!
            return false;
        return true;
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
