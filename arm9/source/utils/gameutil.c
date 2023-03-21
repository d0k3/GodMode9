#include "gameutil.h"
#include "nandcmac.h"
#include "disadiff.h"
#include "fsutil.h" // for TAD verification
#include "game.h"
#include "nand.h" // so that we can trim NAND images
#include "itcm.h" // we need access to part of the OTP
#include "hid.h"
#include "ui.h"
#include "fs.h"
#include "unittype.h"
#include "aes.h"
#include "sha.h"

// use NCCH crypto defines for everything
#define CRYPTO_DECRYPT  NCCH_NOCRYPTO
#define CRYPTO_ENCRYPT  NCCH_STDCRYPTO

// partitionA path
#define PART_PATH       "D:/partitionA.bin"


u32 GetCbcBlocks(FIL* file, void* buffer, u64 offset, u32 count, u8* titlekey, u8* forced_iv) {
    u8 iv[16] __attribute__((aligned(4)));
    UINT btr;
    
    // sanity, get IV
    if ((count % 0x10) || (offset % 0x10)) return 1; // not possible
    if (forced_iv) memcpy(iv, forced_iv, 0x10);
    else if ((offset < 0x10) || (fvx_lseek(file, offset - 0x10) != FR_OK) ||
        (fvx_read(file, iv, 0x10, &btr) != FR_OK) || (btr != 0x10))
        return 1;
    
    // load data
    if ((fvx_lseek(file, offset) != FR_OK) ||
        (fvx_read(file, buffer, count, &btr) != FR_OK) || (btr != count))
        return 1;
    
    // decrypt
    if (titlekey) {
        setup_aeskey(0x11, titlekey);
        use_aeskey(0x11);
        cbc_decrypt(buffer, buffer, count / 0x10, AES_CNT_TITLEKEY_DECRYPT_MODE, iv);   
    }

    return 0;
}

u32 GetNcchHeaders(NcchHeader* ncch, NcchExtHeader* exthdr, ExeFsHeader* exefs, FIL* file, bool nocrypto) {
    u32 offset_ncch = fvx_tell(file);
    UINT btr;

    if (fvx_read(file, ncch, sizeof(NcchHeader), &btr) != FR_OK) return 1;
    if (nocrypto) {
        ncch->flags[3] = 0x00;
        ncch->flags[7] = (ncch->flags[7] & ~0x21) | 0x04;
    }
    if (ValidateNcchHeader(ncch) != 0) return 1;

    if (exthdr) {
        if (!ncch->size_exthdr) return 1;
        fvx_lseek(file, offset_ncch + NCCH_EXTHDR_OFFSET);
        if ((fvx_read(file, exthdr, NCCH_EXTHDR_SIZE, &btr) != FR_OK) ||
            (DecryptNcch((u8*) exthdr, NCCH_EXTHDR_OFFSET, NCCH_EXTHDR_SIZE, ncch, NULL) != 0))
            return 1;
    }

    if (exefs) {
        if (!ncch->size_exefs) return 1;
        u32 offset_exefs = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
        fvx_lseek(file, offset_exefs);
        if ((fvx_read(file, exefs, sizeof(ExeFsHeader), &btr) != FR_OK) ||
            (DecryptNcch((u8*) exefs, ncch->offset_exefs * NCCH_MEDIA_UNIT, sizeof(ExeFsHeader), ncch, NULL) != 0) ||
            (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
            return 1;
    }

    return 0;
}

u32 CheckNcchHash(u8* expected, FIL* file, u32 size_data, u32 offset_ncch, NcchHeader* ncch, ExeFsHeader* exefs) {
    u32 offset_data = fvx_tell(file) - offset_ncch;
    u8 hash[32];

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) return 1;

    sha_init(SHA256_MODE);
    for (u32 i = 0; i < size_data; i += STD_BUFFER_SIZE) {
        u32 read_bytes = min(STD_BUFFER_SIZE, (size_data - i));
        UINT bytes_read;
        fvx_read(file, buffer, read_bytes, &bytes_read);
        DecryptNcch(buffer, offset_data + i, read_bytes, ncch, exefs);
        sha_update(buffer, read_bytes);
    }
    sha_get(hash);

    free(buffer);

    return (memcmp(hash, expected, 32) == 0) ? 0 : 1;
}

u32 LoadNcchHeaders(NcchHeader* ncch, NcchExtHeader* exthdr, ExeFsHeader* exefs, const char* path, u32 offset) {
    FIL file;

    // open file, get NCCH header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, offset);
    if (GetNcchHeaders(ncch, exthdr, exefs, &file, false) != 0) {
        fvx_close(&file);
        return 1;
    }
    fvx_close(&file);

    return 0;
}

u32 LoadNcsdHeader(NcsdHeader* ncsd, const char* path) {
    FIL file;
    UINT btr;

    // open file, get NCSD header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, ncsd, sizeof(NcsdHeader), &btr) != FR_OK) ||
        (ValidateNcsdHeader(ncsd) != 0)) {
        fvx_close(&file);
        return 1;
    }
    fvx_close(&file);

    return 0;
}

u32 LoadCiaStub(CiaStub* stub, const char* path) {
    FIL file;
    UINT btr;
    CiaInfo info;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    // first 0x20 byte of CIA header
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, stub, 0x20, &btr) != FR_OK) || (btr != 0x20) ||
        (ValidateCiaHeader(&(stub->header)) != 0)) {
        fvx_close(&file);
        return 1;
    }
    GetCiaInfo(&info, &(stub->header));

    // everything up till content offset
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, stub, info.offset_content, &btr) != FR_OK) || (btr != info.offset_content)) {
        fvx_close(&file);
        return 1;
    }

    fvx_close(&file);
    return 0;
}

u32 LoadExeFsFile(void* data, const char* path, u32 offset, const char* name, u32 size_max, u32* bytes_read) {
    NcchHeader ncch;
    ExeFsHeader exefs;
    FIL file;
    UINT btr;
    u32 ret = 0;

    // open file, get NCCH, ExeFS header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, offset);
    if ((GetNcchHeaders(&ncch, NULL, &exefs, &file, false) != 0) ||
        (!ncch.size_exefs)) {
        fvx_close(&file);
        return 1;
    }

    // load file from exefs
    ExeFsFileHeader* exefile = NULL;
    for (u32 i = 0; i < 10; i++) {
        u32 size = exefs.files[i].size;
        if (!size || (size > size_max)) continue;
        char* exename = exefs.files[i].name;
        if (strncmp(name, exename, 8) == 0) {
            exefile = exefs.files + i;
            break;
        }
    }

    if (exefile) {
        u32 size_exefile = exefile->size;
        u32 offset_exefile = (ncch.offset_exefs * NCCH_MEDIA_UNIT) + sizeof(ExeFsHeader) + exefile->offset;
        fvx_lseek(&file, offset + offset_exefile); // offset to file
        if ((fvx_read(&file, data, size_exefile, &btr) != FR_OK) ||
            (DecryptNcch(data, offset_exefile, size_exefile, &ncch, &exefs) != 0) ||
            (btr != size_exefile)) {
            ret = 1;
        }
    } else ret = 1;

    if (bytes_read) *bytes_read = btr;
    fvx_close(&file);
    return ret;
}

u32 LoadNcchMeta(CiaMeta* meta, const char* path, u64 offset) {
    NcchHeader ncch;
    NcchExtHeader exthdr;

    // get dependencies from exthdr, icon from exeFS
    if ((LoadNcchHeaders(&ncch, &exthdr, NULL, path, offset) != 0) ||
        (BuildCiaMeta(meta, &exthdr, NULL) != 0) ||
        (LoadExeFsFile(meta->smdh, path, offset, "icon", sizeof(meta->smdh), NULL)))
        return 1;

    return 0;
}

u32 LoadTmdFile(TitleMetaData* tmd, const char* path) {
    // first part (TMD stub only) (we need to read the content count first)
    if (fvx_qread(path, tmd, 0, TMD_SIZE_STUB, NULL) != FR_OK)
        return 1;

    // sanity check
    if (getbe16(tmd->content_count) > TMD_MAX_CONTENTS)
        return 1;

    // second part (read full size)
    if (ValidateTmd(tmd) == 0) {        
        if (fvx_qread(path, tmd, 0, TMD_SIZE_N(getbe16(tmd->content_count)), NULL) != FR_OK)
            return 1;
    } else if ((ValidateTwlTmd(tmd) == 0) && (getbe16(tmd->content_count) == 1)) {
        // for TWL: convert to new TMD format
        static const u8 magic[] = { TMD_SIG_TYPE };
        memcpy(tmd->sig_type, magic, sizeof(magic));
        strncpy((char*) tmd->issuer, TMD_ISSUER, 0x40);
        memset(((u8*) tmd) + TMD_SIZE_STUB, 0x00, TMD_SIZE_N(1) - TMD_SIZE_STUB);
        (tmd->contentinfo)->cmd_count[1] = 0x01;
        // convert and take over chunk (SHA-1 hash stays)
        TmdContentChunk* chunk = (TmdContentChunk*) (void*) (tmd + 1);
        if ((fvx_qread(path, chunk, TMD_SIZE_STUB, 0x24, NULL) != FR_OK) ||
            (FixTmdHashes(tmd) != 0))
            return 1;
    }

    return 0;
}

u32 LoadTicketFile(Ticket** ticket, const char* path_tik) {
    if(!ticket) return 1;

    // load and check ticket
    TicketMinimum tmp;
    UINT br;
    if (fvx_qread(path_tik, &tmp, 0, TICKET_MINIMUM_SIZE, &br) != FR_OK)
        return 1;

    // check type of ticket, set size
    u32 tik_size = 0;
    if ((br == TICKET_MINIMUM_SIZE) && (ValidateTicket((Ticket*)&tmp) == 0)) {
        // standard 3DS ticket
        tik_size = GetTicketSize((Ticket*)&tmp);
    } else if ((br == TICKET_TWL_SIZE) && (ValidateTwlTicket((Ticket*)&tmp) == 0)) {
        // TWL ticket
        tik_size = TICKET_COMMON_SIZE;
    } else return 1;

    Ticket* tik = (Ticket*)malloc(tik_size);
    if (!tik) return 1;

    if (br == TICKET_MINIMUM_SIZE) { // standard 3DS ticket
        if ((fvx_qread(path_tik, tik, 0, tik_size, &br) != FR_OK) || (br != tik_size)) {
            free(tik);
            return 1;
        }
    } else { // TWL ticket (just take over title id and key)
        BuildFakeTicket(tik, tmp.title_id);
        memcpy(tik->titlekey, tmp.titlekey, 0x10);
    }

    *ticket = tik;
    return 0;
}

u32 LoadCdnTicketFile(Ticket** ticket, const char* path_cnt) {
    // path points to CDN content file
    char path_cetk[256];
    strncpy(path_cetk, path_cnt, 256);
    path_cetk[255] = '\0';
    char* name_cetk = strrchr(path_cetk, '/');
    if (!name_cetk) return 1; // will not happen
    name_cetk++;
    snprintf(name_cetk, sizeof(path_cetk) - (name_cetk - path_cetk), "cetk");
    // ticket is loaded and validated here
    return LoadTicketFile(ticket, path_cetk);
}

u32 LoadTicketForTitleId(Ticket** ticket, const u64 title_id) {
    u8 tid[8];
    for (u32 i = 0; i < 8; i++)
        tid[7-i] = (title_id >> (i*8)) & 0xFF;

    // ensure remounting the old mount path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;
    
    // path to ticket.db
    char path_ticketdb[32];
    char drv = *path_store;
    snprintf(path_ticketdb, sizeof(path_ticketdb), "%2.2s/dbs/ticket.db",
        ((drv == 'B') || (drv == '5') || (drv == '4')) ? "4:" : "1:");

    // load ticket
    if (!InitImgFS(path_ticketdb) ||
        ((ReadTicketFromDB(PART_PATH, tid, ticket)) != 0))
        *ticket = NULL;

    // remount old path
    InitImgFS(path_bak);

    return (*ticket) ? 0 : 1;
}

u32 GetTmdContentPath(char* path_content, const char* path_tmd) {
    // path_content should be 256 bytes in size!
    
    // get path to TMD first content
    static const u8 dlc_tid_high[] = { DLC_TID_HIGH };

    // content path string
    char* name_content;
    snprintf(path_content, 256, "%s", path_tmd);
    path_content[255] = '\0';
    name_content = strrchr(path_content, '/');
    if (!name_content) return 1; // will not happen
    name_content++;

    // CDN content?
    bool cdn = IdentifyFileType(path_tmd) & (GAME_CDNTMD|GAME_TWLTMD);

    // load TMD file
    TitleMetaData* tmd = (TitleMetaData*) malloc(TMD_SIZE_MAX);
    TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
    if (!tmd) return 1;
    if ((LoadTmdFile(tmd, path_tmd) != 0) || !getbe16(tmd->content_count)) {
        free(tmd);
        return 1;
    }
    snprintf(name_content, 256 - (name_content - path_content), cdn ? "%08lx" :
        (memcmp(tmd->title_id, dlc_tid_high, sizeof(dlc_tid_high)) == 0) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));

    free(tmd);
    return 0;
}

u32 GetTieTmdPath(char* path_tmd, const char* path_tie) {
    char drv[3] = { 0x00 };
    u64 tid64 = 0;

    // this relies on:
    // 1: titleinfo entries are only loaded from mounted [1/4/A/B]:/dbs/title.db
    // 2: filename starts with title id

    // basic sanity check
    if (*path_tie != 'T') return 1;

    // get title id
    if (sscanf(path_tie, "T:/%016llx", &tid64) != 1) return 1;
    u32 tid_high = (u32) ((tid64 >> 32) & 0xFFFFFFFF);
    u32 tid_low = (u32) (tid64 & 0xFFFFFFFF);

    // load TitleDB entry file
    TitleInfoEntry tie;
    if (fvx_qread(path_tie, &tie, 0, sizeof(TitleInfoEntry), NULL) != FR_OK)
        return 1;

    // determine the drive
    const char* mntpath = GetMountPath();
    if (!mntpath || !*mntpath) return 1;
    strncpy(drv, mntpath, 2);
    if (tid_high & 0x8000) {
        if (*drv == '1') *drv = '2';
        if (*drv == '4') *drv = '5';
        tid_high = 0x00030000 | (tid_high&0xFF);
    }

    // build the path
    snprintf(path_tmd, 64, "%2.2s/title/%08lX/%08lX/content/%08lx.tmd",
        drv, tid_high, tid_low, tie.tmd_content_id);

    // done
    return 0;
}

u32 GetTieContentPath(char* path_content, const char* path_tie) {
    char path_tmd[64];

    // get the TMD path first
    if (GetTieTmdPath(path_tmd, path_tie) != 0)
        return 1;

    // let the TMD content path function take over
    return GetTmdContentPath(path_content, path_tmd);
}

u32 GetTitleIdTmdPath(char* path_tmd, const u64 title_id, bool from_emunand) {
    u32 tid_high = (u32) ((title_id >> 32) & 0xFFFFFFFF);
    u32 tid_low = (u32) (title_id & 0xFFFFFFFF);
    char* drv = from_emunand ?
        ((tid_high & 0x8000) ? "5:" : ((tid_high & 0x10) ? "4:" : "B:")) :
        ((tid_high & 0x8000) ? "2:" : ((tid_high & 0x10) ? "1:" : "A:"));
    if (tid_high & 0x8000) tid_high = 0x00030000 | (tid_high&0xFF);

    char path_pat[64];
    snprintf(path_pat, sizeof(path_pat), "%2.2s/title/%08lX/%08lX/content/*.tmd",
        drv, tid_high, tid_low);

    if (fvx_findpath(path_tmd, path_pat, FN_HIGHEST) != FR_OK)
        return 1;

    // done
    return 0;
}

u32 GetTicketContentPath(char* path_content, const char* path_tik) {
    char path_tmd[256];
    bool from_emunand = false;
    u64 tid64 = 0;

    // available for all tickets, but will fail for titles not installed

    // from mounted ticket.db?
    if (*path_tik == 'T') {
        const char* mntpath = GetMountPath();
        from_emunand = (mntpath && (*mntpath == '4'));
    }

    // load ticket, get title id
    Ticket ticket;
    if (fvx_qread(path_tik, &ticket, 0, sizeof(Ticket), NULL) != FR_OK)
        return 1;
    tid64 = getbe64(ticket.title_id);

    // get the TMD path
    if (GetTitleIdTmdPath(path_tmd, tid64, from_emunand) != 0)
        return 1;

    // let the TMD content path function take over
    return GetTmdContentPath(path_content, path_tmd);
}

u32 WriteCiaStub(CiaStub* stub, const char* path) {
    FIL file;
    UINT btw;
    CiaInfo info;

    GetCiaInfo(&info, &(stub->header));

    // everything up till content offset
    if (fvx_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_write(&file, stub, info.offset_content, &btw) != FR_OK) || (btw != info.offset_content)) {
        fvx_close(&file);
        return 1;
    }

    fvx_close(&file);
    return 0;
}

u32 VerifyTmdContent(const char* path, u64 offset, TmdContentChunk* chunk, const u8* titlekey) {
    u8 hash[32] = { 0 };
    u8 ctr[16];
    FIL file;

    u8* expected = chunk->hash;
    u64 size = getbe64(chunk->size);
    bool encrypted = getbe16(chunk->type) & 0x1;

    if (!ShowProgress(0, 0, path)) return 1;
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    if (offset + size > fvx_size(&file)) {
        fvx_close(&file);
        return 1;
    }
    fvx_lseek(&file, offset);

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) {
        fvx_close(&file);
        return 1;
    }

    u32 mode = SHA1_MODE;
    for (u32 i = 20; i < 32; i++)
        if (expected[i]) mode = SHA256_MODE;
    GetTmdCtr(ctr, chunk);
    sha_init(mode);
    for (u32 i = 0; i < size; i += STD_BUFFER_SIZE) {
        u32 read_bytes = min(STD_BUFFER_SIZE, (size - i));
        UINT bytes_read;
        fvx_read(&file, buffer, read_bytes, &bytes_read);
        if (encrypted) DecryptCiaContentSequential(buffer, read_bytes, ctr, titlekey);
        sha_update(buffer, read_bytes);
        if (!ShowProgress(i + read_bytes, size, path)) break;
    }
    sha_get(hash);
    free(buffer);
    fvx_close(&file);

    return memcmp(hash, expected, 32);
}

u32 VerifyNcchFile(const char* path, u32 offset, u32 size) {
    static bool cryptofix_always = false;
    bool cryptofix = false;
    NcchHeader ncch;
    NcchExtHeader exthdr;
    ExeFsHeader exefs;
    FIL file;

    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // open file, get NCCH, ExeFS header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    // fetch and check NCCH header
    fvx_lseek(&file, offset);
    if (GetNcchHeaders(&ncch, NULL, NULL, &file, cryptofix) != 0) {
        if (!offset) ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_NOT_NCCH_FILE);
        fvx_close(&file);
        return 1;
    }

    // check NCCH size
    if (!size) size = fvx_size(&file) - offset;
    if ((fvx_size(&file) < offset) || (size < ncch.size * NCCH_MEDIA_UNIT)) {
        if (!offset) ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_FILE_IS_TOO_SMALL);
        fvx_close(&file);
        return 1;
    }

    // fetch and check ExeFS header
    fvx_lseek(&file, offset);
    if (ncch.size_exefs && (GetNcchHeaders(&ncch, NULL, &exefs, &file, cryptofix) != 0)) {
        bool borkedflags = false;
        if (ncch.size_exefs && NCCH_ENCRYPTED(&ncch)) {
            // disable crypto, try again
            cryptofix = true;
            fvx_lseek(&file, offset);
            if (GetNcchHeaders(&ncch, NULL, &exefs, &file, cryptofix) == 0) {
                if (cryptofix_always) borkedflags = true;
                else {
                    const char* optionstr[3] = { STR_ATTEMPT_FIX_THIS_TIME, STR_ATTEMPT_FIX_ALWAYS, STR_ABORT_VERIFICATION };
                    u32 user_select = ShowSelectPrompt(3, optionstr, "%s\n%s", pathstr, STR_ERROR_BAD_CRYPTO_FLAGS);
                    if ((user_select == 1) || (user_select == 2)) borkedflags = true;
                    if (user_select == 2) cryptofix_always = true;
                }
            }
        }
        if (!borkedflags) {
            if (!offset) ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_BAD_EXEFS_HEADER);
            fvx_close(&file);
            return 1;
        }
    }

    // fetch and check ExtHeader
    fvx_lseek(&file, offset);
    if (ncch.size_exthdr && (GetNcchHeaders(&ncch, &exthdr, NULL, &file, cryptofix) != 0)) {
        if (!offset) ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_MISSING_EXTHEADER);
        fvx_close(&file);
        return 1;
    }

    // check / setup crypto
    if (SetupNcchCrypto(&ncch, NCCH_NOCRYPTO) != 0) {
        if (!offset) ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_CRYPTO_NOT_SET_UP);
        fvx_close(&file);
        return 1;
    }

    u32 ver_exthdr = 0;
    u32 ver_exefs = 0;
    u32 ver_romfs = 0;

    // base hash check for extheader
    if (ncch.size_exthdr > 0) {
        fvx_lseek(&file, offset + NCCH_EXTHDR_OFFSET);
        ver_exthdr = CheckNcchHash(ncch.hash_exthdr, &file, 0x400, offset, &ncch, NULL);
    }

    // base hash check for exefs
    if (ncch.size_exefs > 0) {
        fvx_lseek(&file, offset + (ncch.offset_exefs * NCCH_MEDIA_UNIT));
        ver_exefs = CheckNcchHash(ncch.hash_exefs, &file, ncch.size_exefs_hash * NCCH_MEDIA_UNIT, offset, &ncch, &exefs);
    }

    // base hash check for romfs
    if (ncch.size_romfs > 0) {
        fvx_lseek(&file, offset + (ncch.offset_romfs * NCCH_MEDIA_UNIT));
        ver_romfs = CheckNcchHash(ncch.hash_romfs, &file, ncch.size_romfs_hash * NCCH_MEDIA_UNIT, offset, &ncch, NULL);
    }

    // thorough exefs verification (workaround for Process9)
    if (!ShowProgress(0, 0, path)) return 1;
    if ((ncch.size_exefs > 0) && (memcmp(exthdr.name, "Process9", 8) != 0)) {
        for (u32 i = 0; !ver_exefs && (i < 10); i++) {
            ExeFsFileHeader* exefile = exefs.files + i;
            u8* hash = exefs.hashes[9 - i];
            if (!exefile->size) continue;
            fvx_lseek(&file, offset + (ncch.offset_exefs * NCCH_MEDIA_UNIT) + 0x200 + exefile->offset);
            ver_exefs = CheckNcchHash(hash, &file, exefile->size, offset, &ncch, &exefs);
        }
    }

    // thorough romfs verification
    if (!ver_romfs && (ncch.size_romfs > 0)) {
        UINT btr;

        // load ivfc header
        RomFsIvfcHeader ivfc;
        fvx_lseek(&file, offset + (ncch.offset_romfs * NCCH_MEDIA_UNIT));
        if ((fvx_read(&file, &ivfc, sizeof(RomFsIvfcHeader), &btr) != FR_OK) ||
            (DecryptNcch((u8*) &ivfc, ncch.offset_romfs * NCCH_MEDIA_UNIT, sizeof(RomFsIvfcHeader), &ncch, NULL) != 0) )
            ver_romfs = 1;

        // load data
        u64 lvl1_size = 0;
        u64 lvl2_size = 0;
        u8* masterhash = NULL;
        u8* lvl1_data = NULL;
        u8* lvl2_data = NULL;
        if (!ver_romfs && (ValidateRomFsHeader(&ivfc, ncch.size_romfs * NCCH_MEDIA_UNIT) == 0)) {
            // load masterhash(es)
            masterhash = malloc(ivfc.size_masterhash);
            if (masterhash) {
                u64 offset_add = (ncch.offset_romfs * NCCH_MEDIA_UNIT) + sizeof(RomFsIvfcHeader);
                fvx_lseek(&file, offset + offset_add);
                if ((fvx_read(&file, masterhash, ivfc.size_masterhash, &btr) != FR_OK) ||
                    (DecryptNcch(masterhash, offset_add, ivfc.size_masterhash, &ncch, NULL) != 0))
                    ver_romfs = 1;
            }

            // load lvl1
            lvl1_size = align(ivfc.size_lvl1, 1 << ivfc.log_lvl1);
            lvl1_data = malloc(lvl1_size);
            if (lvl1_data) {
                u64 offset_add = (ncch.offset_romfs * NCCH_MEDIA_UNIT) + GetRomFsLvOffset(&ivfc, 1);
                fvx_lseek(&file, offset + offset_add);
                if ((fvx_read(&file, lvl1_data, lvl1_size, &btr) != FR_OK) ||
                    (DecryptNcch(lvl1_data, offset_add, lvl1_size, &ncch, NULL) != 0))
                    ver_romfs = 1;
            }

            // load lvl2
            lvl2_size = align(ivfc.size_lvl2, 1 << ivfc.log_lvl2);
            lvl2_data = malloc(lvl2_size);
            if (lvl2_data) {
                u64 offset_add = (ncch.offset_romfs * NCCH_MEDIA_UNIT) + GetRomFsLvOffset(&ivfc, 2);
                fvx_lseek(&file, offset + offset_add);
                if ((fvx_read(&file, lvl2_data, lvl2_size, &btr) != FR_OK) ||
                    (DecryptNcch(lvl2_data, offset_add, lvl2_size, &ncch, NULL) != 0))
                    ver_romfs = 1;
            }

            // check mallocs
            if (!masterhash || !lvl1_data || !lvl2_data)
                ver_romfs = 1; // should never happen
        }

        // actual verification
        if (!ver_romfs) {
            // verify lvl1
            u32 n_blocks = lvl1_size >> ivfc.log_lvl1;
            u32 block_log = ivfc.log_lvl1;
            for (u32 i = 0; !ver_romfs && (i < n_blocks); i++)
                ver_romfs = (u32) sha_cmp(masterhash + (i*0x20), lvl1_data + (i<<block_log), 1<<block_log, SHA256_MODE);

            // verify lvl2
            n_blocks = lvl2_size >> ivfc.log_lvl2;
            block_log = ivfc.log_lvl2;
            for (u32 i = 0; !ver_romfs && (i < n_blocks); i++) {
                ver_romfs = sha_cmp(lvl1_data + (i*0x20), lvl2_data + (i<<block_log), 1<<block_log, SHA256_MODE);
            }

            // lvl3 verification (this will take long)
            u64 offset_add = (ncch.offset_romfs * NCCH_MEDIA_UNIT) + GetRomFsLvOffset(&ivfc, 3);
            n_blocks = align(ivfc.size_lvl3, 1 << ivfc.log_lvl3) >> ivfc.log_lvl3;
            block_log = ivfc.log_lvl3;
            fvx_lseek(&file, offset + offset_add);
            for (u32 i = 0; !ver_romfs && (i < n_blocks); i++) {
                ver_romfs = CheckNcchHash(lvl2_data + (i*0x20), &file, 1 << block_log, offset, &ncch, NULL);
                offset_add += 1 << block_log;
                if (!(i % 16) && !ShowProgress(i+1, n_blocks, path)) ver_romfs = 1;
            }
        }

        if (masterhash) free(masterhash);
        if (lvl1_data) free(lvl1_data);
        if (lvl2_data) free(lvl2_data);
    }

    if (!offset && (ver_exthdr|ver_exefs|ver_romfs)) { // verification summary
        ShowPrompt(false, STR_PATH_NCCH_VERIFICATION_FAILED_INFO, pathstr,
            (!ncch.size_exthdr) ? "-" : (ver_exthdr == 0) ? STR_OK : STR_FAIL,
            (!ncch.size_exefs) ? "-" : (ver_exefs == 0) ? STR_OK : STR_FAIL,
            (!ncch.size_romfs) ? "-" : (ver_romfs == 0) ? STR_OK : STR_FAIL);
    }

    fvx_close(&file);
    if (cryptofix) fvx_qwrite(path, &ncch, offset, sizeof(NcchHeader), NULL);
    return ver_exthdr|ver_exefs|ver_romfs;
}

u32 VerifyNcsdFile(const char* path) {
    NcsdHeader ncsd;

    // path string
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // load NCSD header
    if (LoadNcsdHeader(&ncsd, path) != 0) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_NOT_NCSD_FILE);
        return 1;
    }

    // validate NCSD contents
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd.partitions + i;
        u32 offset = partition->offset * NCSD_MEDIA_UNIT;
        u32 size = partition->size * NCSD_MEDIA_UNIT;
        if (!size) continue;
        if (VerifyNcchFile(path, offset, size) != 0) {
            ShowPrompt(false, STR_PATH_CONTENT_N_SIZE_AT_OFFSET_VERIFICATION_FAILED,
                pathstr, i, size, offset);
            return 1;
        }
    }

    return 0;
}

u32 VerifyCiaFile(const char* path) {
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    CiaInfo info;
    u8 titlekey[16];

    if (!cia) return 1;

     // path string
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // load CIA stub
    if ((LoadCiaStub(cia, path) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0) ||
        (GetTitleKey(titlekey, (Ticket*)&(cia->ticket)) != 0)) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_PROBABLY_NOT_CIA_FILE);
        free(cia);
        return 1;
    }

    // verify TMD
    if (VerifyTmd(&(cia->tmd)) != 0) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_TMD_PROBABLY_CORRUPTED);
        free(cia);
        return 1;
    }

    // verify contents
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    u8* cnt_index = cia->header.content_index;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        u16 index = getbe16(chunk->index);
        if (!(cnt_index[index/8] & (1 << (7-(index%8))))) continue; // don't check missing contents
        if (VerifyTmdContent(path, next_offset, chunk, titlekey) != 0) {
            ShowPrompt(false, STR_PATH_ID_N_SIZE_AT_OFFSET_VERIFICATION_FAILED,
                pathstr, getbe32(chunk->id), getbe64(chunk->size), next_offset);
            free(cia);
            return 1;
        }
        next_offset += getbe64(chunk->size);
    }

    free(cia);
    return 0;
}

u32 VerifyTmdFile(const char* path, bool cdn) {
    static const u8 dlc_tid_high[] = { DLC_TID_HIGH };
    bool ignore_missing_dlc = false;

    // path string
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // content path string
    char path_content[256];
    char* name_content;
    strncpy(path_content, path, 256);
    path_content[255] = '\0';
    name_content = strrchr(path_content, '/');
    if (!name_content) return 1; // will not happen
    name_content++;

    // load TMD file
    TitleMetaData* tmd = (TitleMetaData*) malloc(TMD_SIZE_MAX);
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    if ((LoadTmdFile(tmd, path) != 0) || (VerifyTmd(tmd) != 0)) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_TMD_PROBABLY_CORRUPTED);
        free(tmd);
        return 1;
    }

    u8 titlekey[0x10] = { 0xFF };
    if (cdn) { // load / build ticket (for titlekey / CDN only)
        Ticket* ticket = NULL;
        if (!((LoadCdnTicketFile(&ticket, path) == 0) ||
             ((ticket = (Ticket*)malloc(TICKET_COMMON_SIZE), ticket != NULL) &&
             (BuildFakeTicket(ticket, tmd->title_id) == 0) &&
             (FindTitleKey(ticket, tmd->title_id) == 0))) ||
            (GetTitleKey(titlekey, ticket) != 0)) {
            ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_CDN_TITLEKEY_NOT_FOUND);
            free(ticket);
            free(tmd);
            return 1;
        }
        free(ticket);
    }

    // verify contents
    u32 content_count = getbe16(tmd->content_count);
    bool dlc = (memcmp(tmd->title_id, dlc_tid_high, sizeof(dlc_tid_high)) == 0);
    u32 res = 0;
    for (u32 i = 0; !res && (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(content_list[i]);
        if (!cdn) chunk->type[1] &= ~0x01; // remove crypto flag
        snprintf(name_content, 256 - (name_content - path_content),
            (cdn) ? "%08lx" : (dlc) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
        TruncateString(pathstr, path_content, 32, 8);
        if (dlc && i && !PathExist(path_content)) {
            if (!ignore_missing_dlc && !ShowPrompt(true, "%s\n%s", pathstr, STR_DLC_CONTENT_IS_MISSING_IGNORE_ALL_AND_CONTINUE)) res = 1;
            ignore_missing_dlc = true;
            continue;
        }
        if (VerifyTmdContent(path_content, 0, chunk, titlekey) != 0) {
            ShowPrompt(false, "%s\n%s", pathstr, PathExist(path_content) ? STR_VERIFICATION_FAILED : STR_CONTENT_IS_MISSING);
            res = 1;
        }
    }

    free(tmd);
    return res;
}

u32 VerifyTieFile(const char* path) {
    char path_tmd[64];

    // get the TMD path
    if (GetTieTmdPath(path_tmd, path) != 0)
        return 1;

    // let the TMD verificator take over
    return VerifyTmdFile(path_tmd, false);
}

u32 VerifyTadFile(const char* path) {
    TadStub tad;
    TadHeader* hdr = &(tad.header);
    TadFooter* ftr = &(tad.footer);

    // only works for GM9 decrypted TAD files
    if (!ShowProgress(0, 0, path)) return 1;
    if ((fvx_qread(path, &tad, 0, sizeof(TadStub), NULL) != FR_OK) ||
        (VerifyTadStub(&tad) != 0))
        return 1;

    // verify contents
    u32 content_start = sizeof(TadStub); 
    for (u32 i = 0; i < TAD_NUM_CONTENT; i++) {
        u8 hash[32];
        u32 len = align(hdr->content_size[i], 0x10);
        if (!len) continue; // non-existant section
        if (!FileGetSha(path, hash, content_start, len, false) ||
            (memcmp(hash, ftr->content_sha256[i], 32) != 0))
            return 1;
        content_start += len + sizeof(TadBlockMetaData);
    }

    return 0;
}

u32 VerifyFirmFile(const char* path) {
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    void* firm_buffer = (void*) malloc(FIRM_MAX_SIZE);
    if (!firm_buffer) return 1;

    // load the whole FIRM into memory
    u32 firm_size = fvx_qsize(path);
    if ((firm_size > FIRM_MAX_SIZE) || (fvx_qread(path, firm_buffer, 0, firm_size, NULL) != FR_OK) ||
        (ValidateFirmHeader(firm_buffer, firm_size) != 0)) {
        free(firm_buffer);
        return 1;
    }

    // hash verify all available sections
    FirmHeader header;
    memcpy(&header, firm_buffer, sizeof(FirmHeader));
    for (u32 i = 0; i < 4; i++) {
        FirmSectionHeader* sct = header.sections + i;
        void* section = ((u8*) firm_buffer) + sct->offset;
        if (!(sct->size)) continue;
        if (sha_cmp(sct->hash, section, sct->size, SHA256_MODE) != 0) {
            ShowPrompt(false, STR_PATH_SECTION_N_HASH_MISMATCH, pathstr, i);
            free(firm_buffer);
            return 1;
        }
    }

    // no arm11 / arm9 entrypoints?
    if (!header.entry_arm9) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ARM9_ENTRYPOINT_IS_MISSING);
        free(firm_buffer);
        return 1;
    } else if (!header.entry_arm11) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_WARNING_ARM11_ENTRYPOINT_IS_MISSING);
    }

    free(firm_buffer);
    return 0;
}

u32 VerifyBossFile(const char* path) {
    BossHeader boss;
    u32 payload_size;
    bool encrypted = false;
    FIL file;
    UINT btr;

    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // read file header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, &boss, sizeof(BossHeader), &btr) != FR_OK) ||
        (btr != sizeof(BossHeader)) || (ValidateBossHeader(&boss, 0) != 0)) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_NOT_A_BOSS_FILE);
        fvx_close(&file);
        return 1;
    }

    // get / check size
    payload_size = getbe32(boss.filesize) - sizeof(BossHeader);
    if (!payload_size) {
        fvx_close(&file);
        return 1;
    }

    // check if encrypted, decrypt if required
    encrypted = (CheckBossEncrypted(&boss) == 0);
    if (encrypted) CryptBoss((void*) &boss, 0, sizeof(BossHeader), &boss);

    // set up a buffer
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) {
        fvx_close(&file);
        return 1;
    }

    // actual hash calculation & compare
    u8 hash[32];
    sha_init(SHA256_MODE);

    GetBossPayloadHashHeader(buffer, &boss);
    u32 read_bytes = min((STD_BUFFER_SIZE - BOSS_SIZE_PAYLOAD_HEADER), payload_size);
    fvx_read(&file, buffer + BOSS_SIZE_PAYLOAD_HEADER, read_bytes, &btr);
    if (encrypted) CryptBoss(buffer + BOSS_SIZE_PAYLOAD_HEADER, sizeof(BossHeader), read_bytes, &boss);
    sha_update(buffer, read_bytes + BOSS_SIZE_PAYLOAD_HEADER);

    for (u32 i = read_bytes; i < payload_size; i += STD_BUFFER_SIZE) {
        read_bytes = min(STD_BUFFER_SIZE, (payload_size - i));
        fvx_read(&file, buffer, read_bytes, &btr);
        if (encrypted) CryptBoss(buffer, sizeof(BossHeader) + i, read_bytes, &boss);
        sha_update(buffer, read_bytes);
    }

    sha_get(hash);
    fvx_close(&file);
    free(buffer);

    if (memcmp(hash, boss.hash_payload, 0x20) != 0) {
        if (ShowPrompt(true, "%s\n%s", pathstr, STR_BOSS_PAYLOAD_HASH_MISMATCH_TRY_TO_FIX_IT)) {
            // fix hash, reencrypt BOSS header if required, write to file
            memcpy(boss.hash_payload, hash, 0x20);
            if (encrypted) CryptBoss((void*) &boss, 0, sizeof(BossHeader), &boss);
            if (!CheckWritePermissions(path) ||
                (fvx_qwrite(path, &boss, 0, sizeof(BossHeader), NULL) != FR_OK))
                return 1;
        } else return 1;
    }

    return 0;
}

u32 VerifyTicketFile(const char* path) {
    // load ticket
    Ticket* ticket;
    if (LoadTicketFile(&ticket, path) != 0)
        return 1;

    // ticket verification is strict, fake-signed tickets are discarded
    u32 res = ValidateTicketSignature(ticket);
    free(ticket);
    return res;
}

u32 VerifyGameFile(const char* path) {
    u64 filetype = IdentifyFileType(path);
    if (filetype & GAME_CIA)
        return VerifyCiaFile(path);
    else if (filetype & GAME_NCSD)
        return VerifyNcsdFile(path);
    else if (filetype & GAME_NCCH)
        return VerifyNcchFile(path, 0, 0);
    else if (filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD))
        return VerifyTmdFile(path, filetype & (GAME_CDNTMD|GAME_TWLTMD));
    else if (filetype & GAME_TIE)
        return VerifyTieFile(path);
    else if (filetype & GAME_TAD)
        return VerifyTadFile(path);
    else if (filetype & GAME_BOSS)
        return VerifyBossFile(path);
    else if (filetype & SYS_FIRM)
        return VerifyFirmFile(path);
    else if (filetype & GAME_TICKET)
        return VerifyTicketFile(path);
    else return 1;
}

u32 CheckEncryptedNcchFile(const char* path, u32 offset) {
    NcchHeader ncch;
    if (LoadNcchHeaders(&ncch, NULL, NULL, path, offset) != 0)
        return 1;
    return (NCCH_ENCRYPTED(&ncch)) ? 0 : 1;
}

u32 CheckEncryptedNcsdFile(const char* path) {
    NcsdHeader ncsd;

    // load NCSD header
    if (LoadNcsdHeader(&ncsd, path) != 0)
        return 1;

    // check for encryption in NCSD contents
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd.partitions + i;
        u32 offset = partition->offset * NCSD_MEDIA_UNIT;
        if (!partition->size) continue;
        if (CheckEncryptedNcchFile(path, offset) == 0)
            return 0;
    }

    return 1;
}

u32 CheckEncryptedCiaFile(const char* path) {
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    CiaInfo info;

    if (!cia) return 1;

    // load CIA stub
    if ((LoadCiaStub(cia, path) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0)) {
        free(cia);
        return 1;
    }

    // check for encryption in CIA contents
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        if ((getbe16(chunk->type) & 0x1) || (CheckEncryptedNcchFile(path, next_offset) == 0)) {
            free(cia);
            return 0; // encryption found
        }
        next_offset += getbe64(chunk->size);
    }

    free(cia);
    return 1;
}

u32 CheckEncryptedFirmFile(const char* path) {
    FirmHeader header;
    FIL file;
    UINT btr;

    // open file, get FIRM header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, &header, sizeof(FirmHeader), &btr) != FR_OK) ||
        (ValidateFirmHeader(&header, fvx_size(&file)) != 0)) {
        fvx_close(&file);
        return 1;
    }

    // check ARM9 binary for ARM9 loader
    FirmSectionHeader* arm9s = FindFirmArm9Section(&header);
    if (arm9s) {
        FirmA9LHeader a9l;
        fvx_lseek(&file, arm9s->offset);
        if ((fvx_read(&file, &a9l, sizeof(FirmA9LHeader), &btr) == FR_OK) &&
            (ValidateFirmA9LHeader(&a9l) == 0)) {
            fvx_close(&file);
            return 0;
        }
    }

    fvx_close(&file);
    return 1;
}

u32 CheckEncryptedBossFile(const char* path) {
    // get boss header, check if encrypted
    BossHeader boss;
    if (fvx_qread(path, &boss, 0, sizeof(BossHeader), NULL) != FR_OK) return 1;
    return CheckBossEncrypted(&boss);
}

u32 CheckEncryptedGameFile(const char* path) {
    u64 filetype = IdentifyFileType(path);
    if (filetype & GAME_CIA)
        return CheckEncryptedCiaFile(path);
    else if (filetype & GAME_NCSD)
        return CheckEncryptedNcsdFile(path);
    else if (filetype & GAME_NCCH)
        return CheckEncryptedNcchFile(path, 0);
    else if (filetype & GAME_BOSS)
        return CheckEncryptedBossFile(path);
    else if (filetype & SYS_FIRM)
        return CheckEncryptedFirmFile(path);
    else if (filetype & GAME_NUSCDN)
        return 0; // these *should* always be encrypted
    else return 1;
}

u32 CryptNcchNcsdBossFirmFile(const char* orig, const char* dest, u32 mode, u16 crypto,
    u32 offset, u32 size, TmdContentChunk* chunk, const u8* titlekey) { // this line only for CIA contents
    // this will do a simple copy for unencrypted files
    bool inplace = (strncmp(orig, dest, 256) == 0);
    FIL ofile;
    FIL dfile;
    FIL* ofp = &ofile;
    FIL* dfp = (inplace) ? &ofile : &dfile;
    FSIZE_t fsize;

    // FIRM encryption is not possible (yet)
    if ((mode & SYS_FIRM) && (crypto != CRYPTO_DECRYPT))
        return 1;

    // check for BOSS crypto
    bool crypt_boss = ((mode & GAME_BOSS) && (CheckEncryptedBossFile(orig) == 0));
    crypt_boss = ((mode & GAME_BOSS) && (crypt_boss == (crypto == CRYPTO_DECRYPT)));

    // open file(s)
    if (inplace) {
        if (fvx_open(ofp, orig, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
            return 1;
        fvx_lseek(ofp, offset);
    } else {
        if (fvx_open(ofp, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return 1;
        if (fvx_open(dfp, dest, FA_WRITE | (offset ? FA_OPEN_ALWAYS : FA_CREATE_ALWAYS)) != FR_OK) {
            fvx_close(ofp);
            return 1;
        }
        fvx_lseek(ofp, offset);
        fvx_lseek(dfp, offset);
    }

    fsize = fvx_size(ofp); // for progress bar
    if (fsize < offset) return 1;
    if (!size) size = fsize - offset;

    // ensure free space in destination
    if (!inplace) {
        if ((fvx_lseek(dfp, offset + size) != FR_OK) ||
            (fvx_tell(dfp) != offset + size) ||
            (fvx_lseek(dfp, offset) != FR_OK)) {
            fvx_close(ofp);
            fvx_close(dfp);
            return 1;
        }
    }

    // set up buffer
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) {
        fvx_close(ofp);
        fvx_close(dfp);
        return 1;
    }

    u32 ret = 0;
    if (!ShowProgress(offset, fsize, dest)) ret = 1;
    if (mode & (GAME_NCCH|GAME_NCSD|GAME_BOSS|SYS_FIRM|GAME_NDS)) { // for NCCH / NCSD / BOSS / FIRM files
        for (u64 i = 0; (i < size) && (ret == 0); i += STD_BUFFER_SIZE) {
            u32 read_bytes = min(STD_BUFFER_SIZE, (size - i));
            UINT bytes_read, bytes_written;
            if (fvx_read(ofp, buffer, read_bytes, &bytes_read) != FR_OK) ret = 1;
            if (((mode & GAME_NCCH) && (CryptNcchSequential(buffer, i, read_bytes, crypto) != 0)) ||
                ((mode & GAME_NCSD) && (CryptNcsdSequential(buffer, i, read_bytes, crypto) != 0)) ||
                ((mode & GAME_BOSS) && crypt_boss && (CryptBossSequential(buffer, i, read_bytes) != 0)) ||
                ((mode & SYS_FIRM) && (DecryptFirmSequential(buffer, i, read_bytes) != 0)))
                ret = 1;
            if (inplace) fvx_lseek(ofp, fvx_tell(ofp) - read_bytes);
            if (fvx_write(dfp, buffer, read_bytes, &bytes_written) != FR_OK) ret = 1;
            if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
            if (!ShowProgress(offset + i + read_bytes, fsize, dest)) ret = 1;
        }
    } else if (mode & (GAME_CIA|GAME_NUSCDN)) { // for NCCHs inside CIAs
        bool cia_crypto = getbe16(chunk->type) & 0x1;
        bool ncch_crypto; // find out by decrypting the NCCH header
        UINT bytes_read, bytes_written;
        u8 ctr[16];

        NcchHeader* ncch = (NcchHeader*) (void*) buffer;
        GetTmdCtr(ctr, chunk); // NCCH crypto?
        if (fvx_read(ofp, buffer, sizeof(NcchHeader), &bytes_read) != FR_OK) ret = 1;
        if (cia_crypto) DecryptCiaContentSequential(buffer, sizeof(NcchHeader), ctr, titlekey);
        ncch_crypto = ((ValidateNcchHeader(ncch) == 0) && (NCCH_ENCRYPTED(ncch) || !(crypto & NCCH_NOCRYPTO)));
        if (ncch_crypto && (SetupNcchCrypto(ncch, crypto) != 0))
            ret = 1;

        GetTmdCtr(ctr, chunk);
        fvx_lseek(ofp, offset);
        sha_init(SHA256_MODE);
        for (u64 i = 0; (i < size) && (ret == 0); i += STD_BUFFER_SIZE) {
            u32 read_bytes = min(STD_BUFFER_SIZE, (size - i));
            if (fvx_read(ofp, buffer, read_bytes, &bytes_read) != FR_OK) ret = 1;
            if (cia_crypto && (DecryptCiaContentSequential(buffer, read_bytes, ctr, titlekey) != 0)) ret = 1;
            if (ncch_crypto && (CryptNcchSequential(buffer, i, read_bytes, crypto) != 0)) ret = 1;
            if (inplace) fvx_lseek(ofp, fvx_tell(ofp) - read_bytes);
            if (fvx_write(dfp, buffer, read_bytes, &bytes_written) != FR_OK) ret = 1;
            sha_update(buffer, read_bytes);
            if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
            if (!ShowProgress(offset + i + read_bytes, fsize, dest)) ret = 1;
        }
        sha_get(chunk->hash);
        chunk->type[1] &= ~0x01;
    }

    fvx_close(ofp);
    if (!inplace) fvx_close(dfp);
    if (buffer) free(buffer);

    return ret;
}

u32 CryptCiaFile(const char* orig, const char* dest, u16 crypto) {
    bool inplace = (strncmp(orig, dest, 256) == 0);
    CiaInfo info;
    u8 titlekey[16];

    // start operation
    if (!ShowProgress(0, 0, orig)) return 1;

    // if not inplace: clear destination
    if (!inplace) f_unlink(dest);

    // load CIA stub from origin
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    if (!cia) return 1;
    if ((LoadCiaStub(cia, orig) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0) ||
        (GetTitleKey(titlekey, (Ticket*)&(cia->ticket)) != 0)) {
        free(cia);
        return 1;
    }

    // decrypt CIA contents
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    u8* cnt_index = cia->header.content_index;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        u64 size = getbe64(chunk->size);
        u16 index = getbe16(chunk->index);
        if (!(cnt_index[index/8] & (1 << (7-(index%8))))) continue; // don't crypt missing contents
        if (CryptNcchNcsdBossFirmFile(orig, dest, GAME_CIA, crypto, next_offset, size, chunk, titlekey) != 0) {
            free(cia);
            return 1;
        }
        next_offset += size;
    }

    // if not inplace: take over CIA metadata
    if (!inplace && (info.size_meta == CIA_META_SIZE)) {
        CiaMeta* meta = (CiaMeta*) malloc(sizeof(CiaMeta));
        if (!meta) {
            free(cia);
            return 1;
        }
        if ((fvx_qread(orig, meta, info.offset_meta, CIA_META_SIZE, NULL) != FR_OK) ||
            (fvx_qwrite(dest, meta, info.offset_meta, CIA_META_SIZE, NULL) != FR_OK)) {
            free(cia);
            free(meta);
            return 1;
        }
        free(meta);
    }

    // fix TMD hashes, write CIA stub to destination
    if ((FixTmdHashes(&(cia->tmd)) != 0) || (WriteCiaStub(cia, dest) != 0)) {
        free(cia);
        return 1;
    }

    free(cia);
    return 0;
}

u32 DecryptFirmFile(const char* orig, const char* dest) {
    static const u8 dec_magic[] = { 'D', 'E', 'C', '\0' }; // insert to decrypted firms
    void* firm_buffer = (void*) malloc(FIRM_MAX_SIZE);
    if (!firm_buffer) return 1;

    // load the whole FIRM into memory & decrypt it
    ShowProgress(0, 2, dest);
    u32 firm_size = fvx_qsize(orig);
    if ((firm_size > FIRM_MAX_SIZE) || (fvx_qread(orig, firm_buffer, 0, firm_size, NULL) != FR_OK) ||
        (DecryptFirmFull(firm_buffer, firm_size) != 0)) {
        free(firm_buffer);
        return 1;
    }

    // add the decrypted magic
    FirmHeader* firm = (FirmHeader*) firm_buffer;
    memcpy(firm->dec_magic, dec_magic, sizeof(dec_magic));

    // write decrypted FIRM to the destination file
    ShowProgress(1, 2, dest);
    if (fvx_qwrite(dest, firm_buffer, 0, firm_size, NULL) != FR_OK) {
        free(firm_buffer);
        return 1;
    }

    ShowProgress(2, 2, dest);
    free(firm_buffer);
    return 0;
}

u32 CryptCdnFileBuffered(const char* orig, const char* dest, u16 crypto, void* buffer) {
    TitleMetaData* tmd = (TitleMetaData*) buffer;
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);

    // get name
    char* fname;
    fname = strrchr(orig, '/');
    if (!fname) return 1; // will not happen
    fname++;

    // try to load TMD file
    char path_tmd[256];
    if (!strrchr(fname, '.')) {
        char* name_tmd;
        strncpy(path_tmd, orig, 256);
        path_tmd[255] = '\0';
        name_tmd = strrchr(path_tmd, '/');
        if (!name_tmd) return 1; // will not happen
        name_tmd++;
        snprintf(name_tmd, 256 - (name_tmd - path_tmd), "tmd");
        if (LoadTmdFile(tmd, path_tmd) != 0) tmd = NULL;
    } else tmd = NULL;

    // load or build ticket
    Ticket* ticket = NULL;
    if (LoadCdnTicketFile(&ticket, orig) != 0) {
        if (!tmd || (ticket = (Ticket*)malloc(TICKET_COMMON_SIZE), !ticket)) return 1;
        if ((BuildFakeTicket(ticket, tmd->title_id) != 0)) {
            free(ticket);
            return 1;
        }
        if (FindTitleKey(ticket, tmd->title_id) != 0) {
            free(ticket);
            return 1;
        }
    }

    // get titlekey
    u8 titlekey[0x10] = { 0xFF };
    if (GetTitleKey(titlekey, ticket) != 0) {
        free(ticket);
        return 1;
    }

    free(ticket);

    // find (build fake) content chunk
    TmdContentChunk* chunk = NULL;
    if (!tmd) {
        chunk = content_list;
        memset(chunk, 0, sizeof(TmdContentChunk));
        chunk->type[1] = 0x01; // encrypted
    } else {
        u32 content_count = getbe16(tmd->content_count);
        u32 content_id = 0;
        if (sscanf(fname, "%08lx", &content_id) != 1) return 1;
        for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
            chunk = &(content_list[i]);
            if (getbe32(chunk->id) == content_id) break;
            chunk = NULL;
        }
        if (!chunk || !(getbe16(chunk->type) & 0x01)) return 1;
    }

    // actual crypto
    return CryptNcchNcsdBossFirmFile(orig, dest, GAME_NUSCDN, crypto, 0, 0, chunk, titlekey);
}

u32 CryptCdnFile(const char* orig, const char* dest, u16 crypto) {
    void* buffer = (void*) malloc(TMD_SIZE_MAX);
    if (!buffer) return 1;

    u32 ret = CryptCdnFileBuffered(orig, dest, crypto, buffer);

    free(buffer);
    return ret;
}

u32 CryptGameFile(const char* path, bool inplace, bool encrypt) {
    u64 filetype = IdentifyFileType(path);
    u16 crypto = encrypt ? CRYPTO_ENCRYPT : CRYPTO_DECRYPT;
    char dest[256];
    char* destptr = (char*) path;
    u32 ret = 0;

    if (!inplace) { // build output name
        // build output name
        snprintf(dest, sizeof(dest), OUTPUT_PATH "/");
        char* dname = dest + strnlen(dest, 256);
        if ((strncmp(path + 1, ":/title/", 8) != 0) || (GetGoodName(dname, path, false) != 0)) {
            char* name = strrchr(path, '/');
            if (!name) return 1;
            snprintf(dest, sizeof(dest), "%s/%s", OUTPUT_PATH, ++name);
        }
        destptr = dest;
    }

    if (!CheckWritePermissions(destptr))
        return 1;

    if (!inplace) { // ensure the output dir exists
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
            return 1;
    }

    if (filetype & GAME_CIA)
        ret = CryptCiaFile(path, destptr, crypto);
    else if (filetype & GAME_NUSCDN)
        ret = CryptCdnFile(path, destptr, crypto);
    else if (filetype & SYS_FIRM)
        ret = DecryptFirmFile(path, destptr);
    else if (filetype & (GAME_NCCH|GAME_NCSD|GAME_BOSS))
        ret = CryptNcchNcsdBossFirmFile(path, destptr, filetype, crypto, 0, 0, NULL, NULL);
    else ret = 1;

    if (!inplace && (ret != 0))
        f_unlink(dest); // try to get rid of the borked file

    return ret;
}

u32 GetInstallDataDrive(char* drv, u64 tid64, bool to_emunand) {
    // check the title id
    bool to_twl = ((tid64 >> 32) & 0x8000);
    bool to_sd = (!to_twl && !((tid64 >> 32) & 0x10));

    // sanity
    if (!tid64) return 1;

    // determine the correct drive
    drv[0] = to_emunand ?
        (to_twl ? '5' : to_sd ? 'B' : '4') :
        (to_twl ? '2' : to_sd ? 'A' : '1');
    drv[1] = ':';
    drv[2] = '\0';

    return 0;
}

u32 GetInstallDbsPath(char* path, const char* drv, const char* str) {
    bool is_ticketdb = (strncasecmp(str, "ticket.db", 10) == 0);

    // fix the drive if required
    if (*drv == '2') drv = "1:";
    else if (*drv == '5') drv = "4:";
    else if (is_ticketdb) {
        if (*drv == 'A') drv = "1:";
        else if (*drv == 'B') drv = "4:";
    }

    // build the path
    snprintf(path, 256, "%2.2s/dbs/%s", drv, str);

    return 0;
}

u32 GetInstallPath(char* path, const char* drv, u64 tid64, const u8* content_id, const char* str) {
    static const u8 dlc_tid_high[] = { DLC_TID_HIGH };
    u32 tid_high = (u32) ((tid64 >> 32) & 0xFFFFFFFF);
    u32 tid_low = (u32) (tid64 & 0xFFFFFFFF);
    bool dlc = (tid_high == getbe32(dlc_tid_high));

    if ((*drv == '2') || (*drv == '5')) // TWL titles need TWL title ID
        tid_high = 0x00030000 | (tid_high&0xFF);

    if (content_id) { // app path
        snprintf(path, 256, "%2.2s/title/%08lx/%08lx/content/%s%08lx.app",
            drv, tid_high, tid_low, dlc ? "00000000/" : "", getbe32(content_id));
    } else if (str) { // other paths (TMD/CMD/Save)
        snprintf(path, 256, "%2.2s/title/%08lx/%08lx/%s",
            drv, tid_high, tid_low, str);
    } else { // base path (useful for uninstall)
        snprintf(path, 256, "%2.2s/title/%08lx/%08lx",
            drv, tid_high, tid_low);
    }

    return 0;
}

u32 CreateSaveData(const char* drv, u64 tid64, const char* name, u32 save_size, bool overwrite) {
    bool is_twl = ((*drv == '2') || (*drv == '5'));
    char path_save[128];

    // generate the save path (thanks ihaveamac for system path)
    // we use hardcoded names / numbers for CTR saves
    if ((*drv == '1') || (*drv == '4')) { // ooof, system save
        // get the id0
        u8 sd_keyy[16] __attribute__((aligned(4)));
        char path_movable[32];
        u32 sha256sum[8];
        snprintf(path_movable, sizeof(path_movable), "%2.2s/private/movable.sed", drv);
        if (fvx_qread(path_movable, sd_keyy, 0x110, 0x10, NULL) != FR_OK) return 1;
        memset(sd_keyy, 0x00, 16);
        sha_quick(sha256sum, sd_keyy, 0x10, SHA256_MODE);
        // build path
        u32 tid_low = (u32) (tid64 & 0xFFFFFFFF);
        snprintf(path_save, sizeof(path_save), "%2.2s/data/%08lx%08lx%08lx%08lx/sysdata/%08lx%s",
            drv, sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3],
            tid_low | 0x00020000, name ? "/00000000" : "");
        return 0;
    } else if (!is_twl || !name) { // SD CTR save or no name, simple
        GetInstallPath(path_save, drv, tid64, NULL, name ? "data/00000001.sav" : "data");
    } else {
        char substr[64];
        snprintf(substr, sizeof(substr), "data/%s", name);
        GetInstallPath(path_save, drv, tid64, NULL, substr);
    }

    // if name is NULL, we remove instead of create
    if (!name) {
        fvx_runlink(path_save);
        return 0;
    }

    // generate the save file, first check if it already exists
    if (overwrite || (fvx_qsize(path_save) != save_size)) {
        fvx_rmkpath(path_save);
        if (fvx_qcreate(path_save, save_size) != FR_OK) return 1;

        if (!is_twl) { // CTR save, simple case
            static const u8 zeroes[0x20] = { 0x00 };
            if (fvx_qwrite(path_save, zeroes, 0, 0x20, NULL) != FR_OK)
                return 1;
        } else if ((strncmp(name, "public.sav", 11) == 0) || // fat12 image
                   (strncmp(name, "private.sav", 12) == 0)) {
            u8* fat16k = (u8*) malloc(0x4000); // 16kiB, that's enough
            if (!fat16k) return 1;
            memset(fat16k, 0x00, 0x4000);

            if ((BuildTwlSaveHeader(fat16k, save_size) != 0) ||
                (fvx_qwrite(path_save, fat16k, 0, min(save_size, 0x4000), NULL) != FR_OK)) {
                free(fat16k);
                return 1;
            }
        }
    }

    return 0;
}

u32 UninstallGameData(u64 tid64, bool remove_tie, bool remove_ticket, bool remove_save, bool from_emunand) {
    char drv[3];

    // check permissions for SysNAND (this includes everything we need)
    if (!CheckWritePermissions(from_emunand ? "4:" : "1:")) return 1;

    // determine the drive
    if (GetInstallDataDrive(drv, tid64, from_emunand) != 0) return 1;

    // remove data path
    char path_data[256];
    if (GetInstallPath(path_data, drv, tid64, NULL, remove_save ? NULL : "content") != 0) return 1;
    fvx_runlink(path_data);

    // clear leftovers
    if (GetInstallPath(path_data, drv, tid64, NULL, NULL) != 0)
        fvx_unlink(path_data);

    // remove save (additional step required for system titles)
    if (remove_save && ((*drv == '1') || (*drv == '4')))
        CreateSaveData(drv, tid64, NULL, 0, true);

    // remove titledb entry / ticket
    u32 ret = 0;
    if (remove_tie || remove_ticket) {
        // ensure remounting the old mount path
        char path_store[256] = { 0 };
        char* path_bak = NULL;
        strncpy(path_store, GetMountPath(), 256);
        if (*path_store) path_bak = path_store;

        // we need the big endian title ID
        u8 title_id[8];
        for (u32 i = 0; i < 8; i++)
            title_id[i] = (tid64 >> ((7-i)*8)) & 0xFF;

        // ticket database
        if (remove_ticket) {
            char path_ticketdb[256];
            if ((GetInstallDbsPath(path_ticketdb, drv, "ticket.db") != 0) || !InitImgFS(path_ticketdb) ||
                ((RemoveTicketFromDB(PART_PATH, title_id)) != 0)) ret = 1;
        }

        // title database
        if (remove_tie) {
            char path_titledb[256];
            if ((GetInstallDbsPath(path_titledb, drv, "title.db") != 0) || !InitImgFS(path_titledb) ||
                ((RemoveTitleInfoEntryFromDB(PART_PATH, title_id)) != 0)) ret = 1;
        }

        // restore old mount path
        InitImgFS(path_bak);
    }

    return ret;
}

u32 UninstallGameDataTie(const char* path, bool remove_tie, bool remove_ticket, bool remove_save) {
    // requirements for this to work:
    // * title.db from standard path mounted to T:/
    // * entry filename starts with title id
    // * these two conditions need to be fulfilled for all ties
    bool from_emunand = false;
    u64 tid64;

    const char* mntpath = GetMountPath();
    if (!mntpath) return 1;

    // title.db from emunand?
    if ((strncasecmp(mntpath, "B:/dbs/title.db", 16) == 0) ||
        (strncasecmp(mntpath, "4:/dbs/title.db", 16) == 0))
        from_emunand = true;

    // get title ID
    if (sscanf(path, "T:/%016llx", &tid64) != 1)
        return 1;

    return UninstallGameData(tid64, remove_tie, remove_ticket, remove_save, from_emunand);
}

u32 LoadEncryptedIconFromCiaTmd(const char* path, void* output, void* hdr, bool cia_meta) {
    u64 filetype = IdentifyFileType(path);
    u8 tik_data[16] __attribute__((aligned(32)));
    u8 iv[16] __attribute__((aligned(4)));
    u8* titlekey = NULL;
    u64 offset_cnt = 0;
    char path_cnt[256];
    u8 title_id[8];

    strcpy(path_cnt, path);
    char* name_cnt = strrchr(path_cnt, '/');
    if (!name_cnt) return 0; // will not happen

    void* data = (void*) malloc(max(sizeof(CiaStub), 0x1000));
    if (!data) return 0;
    
    // get content path/offset and TMD data
    TitleMetaData* tmd = (TitleMetaData*) data;
    Ticket* ticket = NULL;
    if ((filetype & GAME_CIA) && (LoadCiaStub(data, path) == 0)) { // CIA file
        CiaStub* cia = (CiaStub*) data;
        CiaInfo info;
        GetCiaInfo(&info, data);
        tmd = &(cia->tmd);
        ticket =(Ticket*) &(cia->ticket);
        offset_cnt = info.offset_content;
        // strcpy(path_cnt, path); // unchanged
    } else if ((filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD)) && (LoadTmdFile(tmd, path) == 0)) {
        const bool cdn = (filetype & (GAME_CDNTMD|GAME_TWLTMD));
        snprintf(++name_cnt, 16, (cdn ? "%08lx" : "%08lx.app"),
            getbe32(((TmdContentChunk*) (tmd+1))->id));
        // offset_cnt = 0; // unchanged
    } else {
        free(data);
        return 0;
    }
    
    // title_id, titlekey & iv
    TmdContentChunk* chunk = (TmdContentChunk*) (tmd+1);
    GetTmdCtr(iv, chunk);
    memcpy(title_id, tmd->title_id, 8);
    if (getbe16(chunk->type) & 0x1) {
        titlekey = tik_data;
        if ((ticket && (GetTitleKey(titlekey, ticket) != 0)) ||
            (FindTitleKeyForId(titlekey, title_id) != 0)) {
            free(data);
            return 0;
        }
    }
    
    // load first block of data
    FIL file;
    if (fvx_open(&file, path_cnt, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        free(data);
        return 0;
    }
    if (GetCbcBlocks(&file, data, offset_cnt, 0x1000, titlekey, iv) != 0) {
        fvx_close(&file);
        free(data);
        return 0;
    }
    
    // find out what it is and proceed
    u32 ret = 0;
    if (!cia_meta && ValidateTwlHeader((TwlHeader*) data) == 0) {
        // TWL data
        TwlHeader* twl = (TwlHeader*) data;
        if (twl->icon_offset && (GetCbcBlocks(&file, (u8*) output,
                offset_cnt + twl->icon_offset, TWLICON_SIZE_DATA(0x0001), titlekey, NULL) == 0) &&
            (VerifyTwlIconData((TwlIconData*) output, 0x0001) == 0))
            ret = GAME_NDS;
    } else if (ValidateNcchHeader((NcchHeader*) data) == 0) {
        // NCCH data
        static const u8 smdh_magic[] = { SMDH_MAGIC };
        NcchHeader* ncch = (NcchHeader*) data;
        u8* icon = NULL;
        if (cia_meta) {
            CiaMeta* meta = (CiaMeta*) output;
            NcchExtHeader* exthdr = (NcchExtHeader*) (void*) (((u8*)data) + NCCH_EXTHDR_OFFSET);
            if (!ncch->size_exthdr || 
                (DecryptNcch(exthdr, NCCH_EXTHDR_OFFSET, NCCH_EXTHDR_SIZE, ncch, NULL) != 0) ||
                (BuildCiaMeta(meta, exthdr, NULL) != 0)) {
                fvx_close(&file);
                free(data);
                return 0;
            }
            icon = (u8*) &(meta->smdh);
        } else icon = (u8*) output;
        memset(icon, 0x00, sizeof(smdh_magic)); // magic number

        if (ncch->size_exefs) {
            ExeFsHeader exefs;
            u64 offset_exefs = ncch->offset_exefs * NCCH_MEDIA_UNIT;
            if ((GetCbcBlocks(&file, &exefs, offset_cnt + offset_exefs, sizeof(ExeFsHeader), titlekey, NULL) == 0) &&
                (DecryptNcch(&exefs, offset_exefs, sizeof(ExeFsHeader), ncch, NULL) == 0) &&
                (ValidateExeFsHeader(&exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) == 0)) {
                ExeFsFileHeader* exefile = NULL;
                for (u32 i = 0; i < 10; i++) {
                    if ((exefs.files[i].size == sizeof(Smdh)) &&
                        (strncmp("icon", exefs.files[i].name, 8) == 0)) {
                        exefile = exefs.files + i;
                        break;
                    }
                }
                if (exefile) {
                    u32 offset_exef = offset_exefs + sizeof(ExeFsHeader) + exefile->offset;
                    if ((GetCbcBlocks(&file, icon, offset_cnt + offset_exef, sizeof(Smdh), titlekey, NULL) == 0) &&
                        (DecryptNcch(icon, offset_exef, sizeof(Smdh), ncch, &exefs) == 0) &&
                        (memcmp(icon, smdh_magic, sizeof(smdh_magic)) == 0))
                        ret = GAME_NCCH;
                }
            }
        }
    }

    // copy header if requested
    if (ret && hdr) memcpy(hdr, data, 0x300);

    fvx_close(&file);
    free(data);
    return ret;
}

u32 InstallCiaContent(const char* drv, const char* path_content, u32 offset, u32 size,
    TmdContentChunk* chunk, const u8* title_id, const u8* titlekey, bool cxi_fix, bool cdn_decrypt) {
    char dest[256];

    // create destination path and ensure it exists
    GetInstallPath(dest, drv, getbe64(title_id), chunk->id, NULL);
    fvx_rmkpath(dest);

    // open file(s)
    FIL ofile;
    FIL dfile;
    FSIZE_t fsize;
    UINT bytes_read, bytes_written;
    if (fvx_open(&ofile, path_content, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&ofile, offset);
    fsize = fvx_size(&ofile);
    if (offset > fsize) return 1;
    if (!size) size = fsize - offset;
    if (fvx_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        fvx_close(&ofile);
        return 1;
    }

    // ensure free space for destination file
    if ((fvx_lseek(&dfile, size) != FR_OK) ||
        (fvx_tell(&dfile) != size) ||
        (fvx_lseek(&dfile, 0) != FR_OK)) {
        fvx_close(&ofile);
        fvx_close(&dfile);
        fvx_unlink(dest);
        return 1;
    }

    // allocate buffer
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) {
        fvx_close(&ofile);
        fvx_close(&dfile);
        fvx_unlink(dest);
        return 1;
    }

    // main loop starts here
    u8 ctr_in[16];
    u8 ctr_out[16];
    u32 ret = 0;
    bool cia_crypto = getbe16(chunk->type) & 0x1;
    GetTmdCtr(ctr_in, chunk);
    GetTmdCtr(ctr_out, chunk);
    if (!ShowProgress(0, 0, path_content)) ret = 1;
    for (u32 i = 0; (i < size) && (ret == 0); i += STD_BUFFER_SIZE) {
        u32 read_bytes = min(STD_BUFFER_SIZE, (size - i));
        if (fvx_read(&ofile, buffer, read_bytes, &bytes_read) != FR_OK) ret = 1;
        if ((cia_crypto || cdn_decrypt) && (DecryptCiaContentSequential(buffer, read_bytes, ctr_in, titlekey) != 0)) ret = 1;
        if ((i == 0) && cxi_fix && (SetNcchSdFlag(buffer) != 0)) ret = 1;
        if (i == 0) sha_init(SHA256_MODE);
        sha_update(buffer, read_bytes);
        if (fvx_write(&dfile, buffer, read_bytes, &bytes_written) != FR_OK) ret = 1;
        if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
        if (!ShowProgress(offset + i + read_bytes, fsize, path_content)) ret = 1;
    }
    u8 hash[0x20];
    sha_get(hash);

    free(buffer);
    fvx_close(&ofile);
    fvx_close(&dfile);

    // did something go wrong?
    if (ret != 0) fvx_unlink(dest);

    // chunk size / chunk hash
    for (u32 i = 0; i < 8; i++) chunk->size[i] = (u8) (size >> (8*(7-i)));
    memcpy(chunk->hash, hash, 0x20);

    return ret;
}

u32 InstallCiaSystemData(CiaStub* cia, const char* drv) {
    // this assumes contents already installed(!)
    // we use hardcoded IDs for CMD (0x1), TMD (0x0), save (0x1/0x0)
    TitleInfoEntry tie;
    CmdHeader* cmd;
    TicketCommon* ticket = &(cia->ticket);
    TitleMetaData* tmd = &(cia->tmd);
    TmdContentChunk* content_list = cia->content_list;
    u32 content_count = getbe16(tmd->content_count);
    u8* title_id = ticket->title_id;
    u64 tid64 = getbe64(title_id);

    bool sdtie = ((*drv == 'A') || (*drv == 'B'));
    bool syscmd = (((*drv == '1') || (*drv == '4')) ||
        (((*drv == '2') || (*drv == '5')) && (title_id[3] != 0x04)));
    bool to_emunand = ((*drv == 'B') || (*drv == '4') || (*drv == '5'));

    char path_titledb[32];
    char path_ticketdb[32];
    char path_tmd[64];
    char path_cmd[64];

    // sanity checks
    if (content_count == 0) return 1;
    if ((*drv != '1') && (*drv != '2') && (*drv != 'A') &&
        (*drv != '4') && (*drv != '5') && (*drv != 'B'))
        return 1;

    // collect data for title info entry
    char path_cnt0[256];
    u8 hdr_cnt0[0x600]; // we don't need more
    NcchHeader* ncch = NULL;
    NcchExtHeader* exthdr = NULL;
    GetInstallPath(path_cnt0, drv, tid64, content_list->id, NULL);
    if (fvx_qread(path_cnt0, hdr_cnt0, 0, 0x600, NULL) != FR_OK)
        return 1;
    if (ValidateNcchHeader((void*) hdr_cnt0) == 0) {
        ncch = (void*) hdr_cnt0;
        exthdr = (void*) (hdr_cnt0 + sizeof(NcchHeader));
        if (!(ncch->size_exthdr) ||
            (DecryptNcch((u8*) exthdr, NCCH_EXTHDR_OFFSET, 0x400, ncch, NULL) != 0))
            exthdr = NULL;
    }

    // build title info entry
    if ((ncch && (BuildTitleInfoEntryNcch(&tie, tmd, ncch, exthdr, sdtie) != 0)) ||
        (!ncch && (BuildTitleInfoEntryTwl(&tie, tmd, (TwlHeader*) (void*) hdr_cnt0) != 0)))
        return 1;

    // build the cmd
    cmd = BuildAllocCmdData(tmd);
    if (!cmd) return 1;
    if (!syscmd) cmd->unknown = 0xFFFFFFFE; // mark this as custom built

    // generate all the paths
    snprintf(path_titledb, sizeof(path_titledb), "%2.2s/dbs/title.db",
        (*drv == '2') ? "1:" : *drv == '5' ? "4:" : drv);
    snprintf(path_ticketdb, sizeof(path_ticketdb), "%2.2s/dbs/ticket.db",
        ((*drv == 'A') || (*drv == '2')) ? "1:" :
        ((*drv == 'B') || (*drv == '5')) ? "4:" : drv);
    GetInstallPath(path_tmd, drv, tid64, NULL, "content/00000000.tmd");
    GetInstallPath(path_cmd, drv, tid64, NULL, "content/cmd/00000001.cmd");

    // copy tmd & cmd
    fvx_rmkpath(path_tmd);
    fvx_rmkpath(path_cmd);
    if ((fvx_qwrite(path_tmd, tmd, 0, TMD_SIZE_N(content_count), NULL) != FR_OK) ||
        (fvx_qwrite(path_cmd, cmd, 0, CMD_SIZE(cmd), NULL) != FR_OK)) {
        free(cmd);
        return 1;
    }
    free(cmd); // we don't need this anymore

    // generate savedata
    u32 save_size = getle32(tmd->save_size);
    u32 twl_privsave_size = getle32(tmd->twl_privsave_size);
    if (exthdr && save_size && // NCCH
        (CreateSaveData(drv, tid64, "*", save_size, false) != 0))
        return 1;
    if (!ncch && save_size && // TWL public.sav
        (CreateSaveData(drv, tid64, "public.sav", save_size, false) != 0))
        return 1;
    if (!ncch && twl_privsave_size && // TWL private.sav
        (CreateSaveData(drv, tid64, "private.sav", twl_privsave_size, false) != 0))
        return 1;
    if ((tmd->twl_flag & 0x2) && // TWL banner.sav
        (CreateSaveData(drv, tid64, "banner.sav", sizeof(TwlIconData), false) != 0))
        return 1;

    // install seed to system (if available)
    if (ncch && (SetupSystemForNcch(ncch, to_emunand) != 0))
        return 1;

    // write ticket and title databases
    // ensure remounting the old mount path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;

    // title database
    if (!InitImgFS(path_titledb) ||
        (AddTitleInfoEntryToDB(PART_PATH, title_id, &tie, true) != 0)) {
        InitImgFS(path_bak);
        return 1;
    }

    // ticket database
    if (!InitImgFS(path_ticketdb) ||
        (AddTicketToDB(PART_PATH, title_id, (Ticket*) ticket, true) != 0)) {
        // workaround for bug #685
        RemoveTicketFromDB(PART_PATH, title_id);
        if (AddTicketToDB(PART_PATH, title_id, (Ticket*) ticket, true) != 0) {
            InitImgFS(path_bak);
            return 1;
        }
    }

    // restore old mount path
    InitImgFS(path_bak);

    // fix CMACs where required
    if (!syscmd) FixFileCmac(path_cmd, true);

    return 0;
}

u32 InsertCiaContent(const char* path_cia, const char* path_content, u32 offset, u32 size,
    TmdContentChunk* chunk, const u8* titlekey, bool force_legit, bool cxi_fix, bool cdn_decrypt) {
    // crypto types / ctr
    bool ncch_decrypt = !force_legit;
    bool cia_encrypt = (force_legit && (getbe16(chunk->type) & 0x01));
    if (!cia_encrypt) chunk->type[1] &= ~0x01; // remove crypto flag

    // open file(s)
    FIL ofile;
    FIL dfile;
    FSIZE_t fsize;
    UINT bytes_read, bytes_written;
    if (fvx_open(&ofile, path_content, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&ofile, offset);
    fsize = fvx_size(&ofile);
    if (offset > fsize) return 1;
    if (!size) size = fsize - offset;
    if (fvx_open(&dfile, path_cia, FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        fvx_close(&ofile);
        return 1;
    }

    // ensure free space for destination file
    UINT offset_dest = fvx_size(&dfile);
    if ((fvx_lseek(&dfile, offset_dest + size) != FR_OK) ||
        (fvx_tell(&dfile) != offset_dest + size) ||
        (fvx_lseek(&dfile, offset_dest) != FR_OK)) {
        fvx_close(&ofile);
        fvx_close(&dfile);
        return 1;
    }

    // check if NCCH crypto is available
    if (ncch_decrypt) {
        NcchHeader ncch;
        u8 ctr[16];
        GetTmdCtr(ctr, chunk);
        if ((fvx_read(&ofile, &ncch, sizeof(NcchHeader), &bytes_read) != FR_OK) ||
            (cdn_decrypt && (DecryptCiaContentSequential((u8*) &ncch, 0x200, ctr, titlekey) != 0)) ||
            (ValidateNcchHeader(&ncch) != 0) ||
            (SetupNcchCrypto(&ncch, NCCH_NOCRYPTO) != 0))
            ncch_decrypt = false;
        fvx_lseek(&ofile, offset);
    }

    // allocate buffer
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) {
        fvx_close(&ofile);
        fvx_close(&dfile);
        return 1;
    }

    // main loop starts here
    u8 ctr_in[16];
    u8 ctr_out[16];
    u32 ret = 0;
    GetTmdCtr(ctr_in, chunk);
    GetTmdCtr(ctr_out, chunk);
    if (!ShowProgress(0, 0, path_content)) ret = 1;
    for (u32 i = 0; (i < size) && (ret == 0); i += STD_BUFFER_SIZE) {
        u32 read_bytes = min(STD_BUFFER_SIZE, (size - i));
        if (fvx_read(&ofile, buffer, read_bytes, &bytes_read) != FR_OK) ret = 2;
        if (cdn_decrypt && (DecryptCiaContentSequential(buffer, read_bytes, ctr_in, titlekey) != 0)) ret = 1;
        if (ncch_decrypt && (DecryptNcchSequential(buffer, i, read_bytes) != 0)) ret = 1;
        if ((i == 0) && cxi_fix && (SetNcchSdFlag(buffer) != 0)) ret = 1;
        if (i == 0) sha_init(SHA256_MODE);
        sha_update(buffer, read_bytes);
        if (cia_encrypt && (EncryptCiaContentSequential(buffer, read_bytes, ctr_out, titlekey) != 0)) ret = 1;
        if (fvx_write(&dfile, buffer, read_bytes, &bytes_written) != FR_OK) ret = 1;
        if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
        if (!ShowProgress(offset + i + read_bytes, fsize, path_content)) ret = 1;
    }
    u8 hash[0x20] __attribute__((aligned(4)));
    sha_get(hash);

    free(buffer);
    fvx_close(&ofile);
    fvx_close(&dfile);

    // force legit?
    if (force_legit && (memcmp(hash, chunk->hash, 0x20) != 0)) return 2;
    if (force_legit && (getbe64(chunk->size) != size)) return 2;

    // chunk size / chunk hash
    for (u32 i = 0; i < 8; i++) chunk->size[i] = (u8) (size >> (8*(7-i)));
    memcpy(chunk->hash, hash, 0x20);

    return ret;
}

u32 InsertCiaMeta(const char* path_cia, CiaMeta* meta) {
    FIL file;
    UINT btw;
    if (fvx_open(&file, path_cia, FA_WRITE | FA_OPEN_APPEND) != FR_OK)
        return 1;
    bool res = ((fvx_write(&file, meta, CIA_META_SIZE, &btw) == FR_OK) && (btw == CIA_META_SIZE));
    fvx_close(&file);
    return (res) ? 0 : 1;
}

u32 InstallFromCiaFile(const char* path_cia, const char* path_dest) {
    CiaInfo info;
    u8 titlekey[16];

    // start operation
    if (!ShowProgress(0, 0, path_cia)) return 1;

    // load CIA stub from origin
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    if (!cia) return 1;
    if ((LoadCiaStub(cia, path_cia) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0) ||
        (GetTitleKey(titlekey, (Ticket*)&(cia->ticket)) != 0)) {
        free(cia);
        return 1;
    }

    // install CIA contents
    u8* title_id = cia->tmd.title_id;
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    u8* cnt_index = cia->header.content_index;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        u64 size = getbe64(chunk->size);
        u16 index = getbe16(chunk->index);
        if (!(cnt_index[index/8] & (1 << (7-(index%8))))) continue; // don't try to install missing contents
        if (InstallCiaContent(path_dest, path_cia, next_offset, size,
            chunk, title_id, titlekey, false, false) != 0) {
            free(cia);
            return 1;
        }
        next_offset += size;
    }

    // fix for CIA console ID (if device ID different)
    if (getbe32(cia->ticket.console_id) != (&ARM9_ITCM->otp)->deviceId)
        memset(cia->ticket.console_id, 0x00, 4);

    // verify TMD hashes, install CIA system data
    if ((VerifyTmd(&(cia->tmd)) != 0) ||
        (InstallCiaSystemData(cia, path_dest) != 0)) {
        free(cia);
        return 1;
    }

    free(cia);
    return 0;
}

u32 BuildCiaLegitTicket(Ticket* ticket, u8* title_id, const char* path_cnt, bool cdn, bool force_legit) {
    bool src_emunand = ((*path_cnt == 'B') || (*path_cnt == '4'));

    if (BuildFakeTicket(ticket, title_id) != 0)
        return 1; 

    if (force_legit) {
        Ticket* ticket_tmp = NULL;
        bool copy = true;

        if ((cdn && (LoadCdnTicketFile(&ticket_tmp, path_cnt) != 0)) ||
            (!cdn && (FindTicket(&ticket_tmp, title_id, true, src_emunand) != 0)) ||
            (GetTicketSize(ticket_tmp) != TICKET_COMMON_SIZE)) {
            FindTitleKey(ticket, title_id);
            copy = false;
        }

        // check the tickets' console id, warn if it isn't zero
        if (copy && getbe32(ticket_tmp->console_id)) {
            static u32 default_action = 0;
            const char* optionstr[2] =
                {STR_GENERIC_TICKET_PIRATE_LEGIT, STR_PERSONALIZED_TICKET_LEGIT};
            if (!default_action) {
                default_action = ShowSelectPrompt(2, optionstr,
                    STR_ID_N_LEGIT_TICKET_IS_PERSONALIZED_USING_THIS_NOT_RECOMMENDED_CHOOSE_DEFAULT_ACTION, getbe64(title_id));
                ShowProgress(0, 0, path_cnt);
            }
            if (!default_action) {
                free(ticket_tmp);
                return 1;
            }
            else if (default_action == 1) {
                memcpy(ticket->titlekey, ticket_tmp->titlekey, 0x10);
                ticket->commonkey_idx = ticket_tmp->commonkey_idx;
                copy = false;
            }
        }

        // copy what we found
        if (copy) memcpy(ticket, ticket_tmp, TICKET_COMMON_SIZE);
        free(ticket_tmp);
    } else if (cdn) {
        Ticket* ticket_tmp = NULL;

        // take over data from CDN ticket into fake ticket
        if (LoadCdnTicketFile(&ticket_tmp, path_cnt) == 0) {
            memcpy(ticket->titlekey, ticket_tmp->titlekey, 0x10);
            ticket->commonkey_idx = ticket_tmp->commonkey_idx;
            free(ticket_tmp);
        } else if (FindTitleKey(ticket, title_id) != 0) {
            ShowPrompt(false, STR_ID_N_TITLEKEY_NOT_FOUND, getbe64(title_id));
            return 1;
        }
    } else {
        Ticket* ticket_tmp = NULL;

        // standard ticket, based on fake ticket
        if ((FindTitleKey(ticket, title_id) != 0) &&
            (FindTicket(&ticket_tmp, title_id, false, src_emunand) == 0)) {
            // we just copy the titlekey from a valid ticket (if we can)
            memcpy(ticket->titlekey, ticket_tmp->titlekey, 0x10);
            ticket->commonkey_idx = ticket_tmp->commonkey_idx;
        }
        if (ticket_tmp) free(ticket_tmp);
    }

    return 0;
}

u32 BuildCiaFromTadFile(const char* path_tad, const char* path_dest, bool force_legit) {
    TadStub tad;
    TadHeader* hdr = &(tad.header);

    // only works for GM9 decrypted TAD files
    // fetch the TAD stub
    if (!ShowProgress(0, 0, path_tad)) return 1;
    if ((fvx_qread(path_tad, &tad, 0, sizeof(TadStub), NULL) != FR_OK) ||
        (VerifyTadStub(&tad) != 0) || (hdr->content_size[0] < TMD_SIZE_N(1)))
        return 1;
    
    // build the CIA stub
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    if (!cia) return 1;
    TitleMetaData* tmd = &(cia->tmd);
    TicketCommon* ticket = &(cia->ticket);
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header), TICKET_COMMON_SIZE) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (fvx_qread(path_tad, tmd, sizeof(TadStub), hdr->content_size[0], NULL) != FR_OK) ||
        (BuildCiaLegitTicket((Ticket*) &(cia->ticket), tmd->title_id, path_tad, false, force_legit) != 0) ||
        (BuildFakeTicket((Ticket*) ticket, tmd->title_id) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, path_dest) != 0)) {
        free(cia);
        return 1;
    }
    
    // extract info from TMD
    u32 content_count = getbe16(tmd->content_count);
    u8* title_id = tmd->title_id;
    if (!content_count || (content_count > 8)) {
        free(cia);
        return 1;
    }
    
    // check for legit TMD
    if (force_legit && ((ValidateTmdSignature(tmd) != 0) || VerifyTmd(tmd) != 0)) {
        ShowPrompt(false, STR_ID_N_TMD_IN_TAD_NOT_LEGIT, getbe64(title_id));
        free(cia);
        return 1;
    }

    // compare TMD with TAD content table
    u32 tad_content_count = 0;
    for (u32 i = 1, ci = 0; i < 9; i++) {
        u32 size_in_tad = hdr->content_size[i];
        if (size_in_tad) {
            u64 size = 0;
            if (ci < content_count) {
                TmdContentChunk* chunk = &(cia->content_list[ci]);
                size = getbe64(chunk->size);
                ci++;
            }
            if (size != size_in_tad) break;
            tad_content_count++;
        }
    }
    if (tad_content_count != content_count) {
        free(cia);
        return 1;
    }
    
    // attempt to find a titlekey
    u8 titlekey[16] = { 0xFF };
    FindTitleKey((Ticket*) ticket, title_id);
    GetTitleKey(titlekey, (Ticket*)&(cia->ticket));

    // insert TAD contents into CIA
    u64 next_offset = sizeof(TadStub) + align(hdr->content_size[0], 0x10) + sizeof(TadBlockMetaData);
    for (u32 i = 0; i < content_count; i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        u64 size = getbe64(chunk->size);
        if (InsertCiaContent(path_dest, path_tad, next_offset, size,
             chunk, titlekey, force_legit, false, false) != 0) {
            free(cia);
            return 1;
        }
        next_offset += align(size, 0x10) + sizeof(TadBlockMetaData);
    }

    // verify TMD / write CIA stub / install system data (take #2)
    if ((force_legit && (VerifyTmd(tmd) != 0)) ||
        (!force_legit && (FixTmdHashes(tmd) != 0)) ||
        (WriteCiaStub(cia, path_dest) != 0)) {
        free(cia);
        return 1;
    }

    free(cia);
    return 0;
}

u32 BuildInstallFromTmdFileBuffered(const char* path_tmd, const char* path_dest, bool force_legit, bool cdn, void* buffer, bool install) {
    const u8 dlc_tid_high[] = { DLC_TID_HIGH };
    static const u8 twl_tid_high[] = { 0x00, 0x03, 0x00, 0x04 };
    static const u8 ctr_tid_high[] = { 0x00, 0x04, 0x80, 0x04 };

    CiaStub* cia = (CiaStub*) buffer;
    TitleMetaData* tmd = &(cia->tmd);
    TmdContentChunk* content_list = cia->content_list;

    // Init progress bar
    if (!ShowProgress(0, 0, path_tmd)) return 1;

    // build the CIA stub
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header), TICKET_COMMON_SIZE) != 0) ||
        (LoadTmdFile(&(cia->tmd), path_tmd) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildCiaLegitTicket((Ticket*) &(cia->ticket), tmd->title_id, path_tmd, cdn, force_legit) != 0))
        return 1;

    // extract info from TMD
    u32 content_count = getbe16(tmd->content_count);
    u8* title_id = tmd->title_id;
    bool dlc = (memcmp(tmd->title_id, dlc_tid_high, sizeof(dlc_tid_high)) == 0);
    if (!content_count) return 1;

    // check for legit TMD
    if (force_legit && ((ValidateTmdSignature(tmd) != 0) || VerifyTmd(tmd) != 0)) {
        ShowPrompt(false, STR_ID_N_TMD_NOT_LEGIT, getbe64(title_id));
        return 1;
    }

    // fix title id/key for faked TWL TMDs & Tickets
    if (memcmp(title_id, twl_tid_high, 3) == 0) {
        u8 titlekey[16];
        memcpy(title_id, ctr_tid_high, 3);
        GetTitleKey(titlekey, (Ticket*) &(cia->ticket));
        memcpy((cia->ticket).title_id, title_id, 8);
        SetTitleKey(titlekey, (Ticket*) &(cia->ticket));
    }

    // content path string
    char path_content[256];
    char* name_content;
    strncpy(path_content, path_tmd, 256);
    path_content[255] = '\0';
    name_content = strrchr(path_content, '/');
    if (!name_content) return 1; // will not happen
    name_content++;

    u8 present[(TMD_MAX_CONTENTS + 7) / 8];
    memset(present, 0xFF, sizeof(present));

    // DLC? Check for missing contents first!
    if (dlc) for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        FILINFO fno;
        TmdContentChunk* chunk = &(content_list[i]);
        TicketRightsCheck rights_ctx;
        TicketRightsCheck_InitContext(&rights_ctx, (Ticket*)&(cia->ticket));
        snprintf(name_content, 256 - (name_content - path_content),
            (cdn) ? "%08lx" : (dlc) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
        if ((fvx_stat(path_content, &fno) != FR_OK) || (fno.fsize != (u32) getbe64(chunk->size)) ||
            (!cdn && !TicketRightsCheck_CheckIndex(&rights_ctx, getbe16(chunk->index)))) {
            present[i / 8] ^= 1 << (i % 8);

            u16 index = getbe16(chunk->index);
            cia->header.size_content -= getbe64(chunk->size);
            cia->header.content_index[index/8] &= ~(1 << (7-(index%8)));
        }
    }

    // insert / install contents
    u32 ret = 0;
    u8 titlekey[16] = { 0xFF };
    if ((GetTitleKey(titlekey, (Ticket*)&(cia->ticket)) != 0) && force_legit) return 1;
    if (!install && (WriteCiaStub(cia, path_dest) != 0)) return 1;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(content_list[i]);
        if (present[i / 8] & (1 << (i % 8))) {
            snprintf(name_content, 256 - (name_content - path_content),
                (cdn) ? "%08lx" : (dlc && !cdn) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
            if (!install && ((ret = InsertCiaContent(path_dest, path_content, 0, (u32) getbe64(chunk->size),
                    chunk, titlekey, force_legit, false, cdn)) != 0)) {
                ShowPrompt(false, STR_ID_N_DOT_N_STATUS, getbe64(title_id), getbe32(chunk->id),
                    (ret == 2) ? STR_CONTENT_IS_CORRUPT : STR_INSERT_CONTENT_FAILED);
                return 1;
            }
            if (install && (InstallCiaContent(path_dest, path_content, 0, (u32) getbe64(chunk->size),
                    chunk, title_id, titlekey, false, cdn) != 0)) {
                ShowPrompt(false, STR_ID_N_DOT_N_STATUS, getbe64(title_id), getbe32(chunk->id), STR_INSTALL_CONTENT_FAILED);
                return 1;
            }
        }
    }

    // try to build & insert meta, but ignore result (from encrypted data?)
    if (!install && content_count) {
        CiaMeta* meta = (CiaMeta*) malloc(sizeof(CiaMeta));
        if (meta) {
            if (cdn) {
                if ((LoadEncryptedIconFromCiaTmd(path_tmd, meta, NULL, true) == GAME_NCCH) &&
                    (InsertCiaMeta(path_dest, meta) == 0))
                    cia->header.size_meta = CIA_META_SIZE;
            } else {
                snprintf(name_content, 256 - (name_content - path_content), "%08lx.app", getbe32(content_list->id));
                if ((LoadNcchMeta(meta, path_content, 0) == 0) && (InsertCiaMeta(path_dest, meta) == 0))
                    cia->header.size_meta = CIA_META_SIZE;
            }
            free(meta);
        }
    }

    // verify TMD / write CIA stub / install system data (take #2)
    if ((force_legit && (VerifyTmd(tmd) != 0)) ||
        (!force_legit && (FixTmdHashes(tmd) != 0)) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0)) ||
        (install && (InstallCiaSystemData(cia, path_dest) != 0)))
        return 1;

    return 0;
}

u32 InstallFromTmdFile(const char* path_tmd, const char* path_dest) {
    void* buffer = (void*) malloc(sizeof(CiaStub));
    if (!buffer) return 1;

    u32 ret = BuildInstallFromTmdFileBuffered(path_tmd, path_dest, false, true, buffer, true);

    free(buffer);
    return ret;
}

u32 BuildCiaFromTmdFile(const char* path_tmd, const char* path_dest, bool force_legit, bool cdn) {
    void* buffer = (void*) malloc(sizeof(CiaStub));
    if (!buffer) return 1;

    u32 ret = BuildInstallFromTmdFileBuffered(path_tmd, path_dest, force_legit, cdn, buffer, false);

    free(buffer);
    return ret;
}

u32 BuildCiaFromTieFile(const char* path_tie, const char* path_dest, bool force_legit) {
    char path_tmd[64];

    // get the TMD path
    if (GetTieTmdPath(path_tmd, path_tie) != 0)
        return 1;

    // let the TMD builder function take over
    return BuildCiaFromTmdFile(path_tmd, path_dest, force_legit, false);
}

u32 BuildInstallFromNcchFile(const char* path_ncch, const char* path_dest, bool install) {
    NcchExtHeader exthdr;
    NcchHeader ncch;
    u8 title_id[8];
    u32 save_size = 0;
    bool has_exthdr = false;

    // Init progress bar
    if (!ShowProgress(0, 0, path_ncch)) return 1;

    // load NCCH header / extheader, get save size && title id
    if (LoadNcchHeaders(&ncch, &exthdr, NULL, path_ncch, 0) == 0) {
        save_size = (u32) exthdr.savedata_size;
        has_exthdr = true;
    } else if (LoadNcchHeaders(&ncch, NULL, NULL, path_ncch, 0) != 0) {
        return 1;
    }
    for (u32 i = 0; i < 8; i++)
        title_id[i] = (ncch.programId >> ((7-i)*8)) & 0xFF;

    // build the CIA stub
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    if (!cia) return 1;
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header), TICKET_COMMON_SIZE) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildFakeTicket((Ticket*)&(cia->ticket), title_id) != 0) ||
        (BuildFakeTmd(&(cia->tmd), title_id, 1, save_size, 0, 0)) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0))) {
        free(cia);
        return 1;
    }

    // insert / install NCCH content
    TmdContentChunk* chunk = cia->content_list;
    memset(chunk, 0, sizeof(TmdContentChunk)); // nothing else to do
    if ((!install && (InsertCiaContent(path_dest, path_ncch, 0, 0, chunk, NULL, false, true, false) != 0)) ||
        (install && (InstallCiaContent(path_dest, path_ncch, 0, 0, chunk, title_id, NULL, true, false) != 0))) {
        free(cia);
        return 1;
    }

    // optional stuff (proper titlekey / meta data)
    if (!install) {
        CiaMeta* meta = (CiaMeta*) malloc(sizeof(CiaMeta));
        if (meta && has_exthdr && (BuildCiaMeta(meta, &exthdr, NULL) == 0) &&
            (LoadExeFsFile(meta->smdh, path_ncch, 0, "icon", sizeof(meta->smdh), NULL) == 0) &&
            (InsertCiaMeta(path_dest, meta) == 0))
            cia->header.size_meta = CIA_META_SIZE;
        free(meta);
    }

    // write the CIA stub (take #2)
    FindTitleKey((Ticket*)(&cia->ticket), title_id);
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0)) ||
        (install && (InstallCiaSystemData(cia, path_dest) != 0))) {
        free(cia);
        return 1;
    }

    free(cia);
    return 0;
}

u32 BuildInstallFromNcsdFile(const char* path_ncsd, const char* path_dest, bool install) {
    NcchExtHeader exthdr;
    NcsdHeader ncsd;
    NcchHeader ncch;
    u8 title_id[8];
    u32 save_size = 0;

    // Init progress bar
    if (!ShowProgress(0, 0, path_ncsd)) return 1;

    // load NCSD header, get content count, title id
    u32 content_count = 0;
    if (LoadNcsdHeader(&ncsd, path_ncsd) != 0) return 1;
    for (u32 i = 0; i < 3; i++)
        if (ncsd.partitions[i].size) content_count++;
    for (u32 i = 0; i < 8; i++)
        title_id[i] = (ncsd.mediaId >> ((7-i)*8)) & 0xFF;

    // load first content NCCH / extheader
    if (LoadNcchHeaders(&ncch, &exthdr, NULL, path_ncsd, NCSD_CNT0_OFFSET) != 0)
        return 1;
    save_size = (u32) exthdr.savedata_size;

    // build the CIA stub
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    if (!cia) return 1;
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header), TICKET_COMMON_SIZE) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildFakeTicket((Ticket*)&(cia->ticket), title_id) != 0) ||
        (BuildFakeTmd(&(cia->tmd), title_id, content_count, save_size, 0, 0)) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0))) {
        free(cia);
        return 1;
    }

    // insert / install NCSD content
    TmdContentChunk* chunk = cia->content_list;
    for (u32 i = 0; i < 3; i++) {
        NcchPartition* partition = ncsd.partitions + i;
        u32 offset = partition->offset * NCSD_MEDIA_UNIT;
        u32 size = partition->size * NCSD_MEDIA_UNIT;
        if (!size) continue;
        memset(chunk, 0, sizeof(TmdContentChunk));
        chunk->id[3] = i;
        chunk->index[1] = i;
        if ((!install && (InsertCiaContent(path_dest, path_ncsd,
                offset, size, chunk++, NULL, false, (i == 0), false) != 0)) ||
            (install && (InstallCiaContent(path_dest, path_ncsd,
                offset, size, chunk++, title_id, NULL, (i == 0), false) != 0))) {
            free(cia);
            return 1;
        }
    }

    // optional stuff (proper titlekey)
    if (!install) {
        CiaMeta* meta = (CiaMeta*) malloc(sizeof(CiaMeta));
        if (meta && (BuildCiaMeta(meta, &exthdr, NULL) == 0) &&
            (LoadExeFsFile(meta->smdh, path_ncsd, NCSD_CNT0_OFFSET, "icon", sizeof(meta->smdh), NULL) == 0) &&
            (InsertCiaMeta(path_dest, meta) == 0))
            cia->header.size_meta = CIA_META_SIZE;
        if (meta) free(meta);
    }

    // update title version from cart header (yeah, that's a bit hacky)
    u16 title_version;
    if (fvx_qread(path_ncsd, &title_version, 0x310, 2, NULL) == FR_OK) {
        u8 title_version_le[2];
        title_version_le[0] = (title_version >> 8) & 0xFF;
        title_version_le[1] = title_version & 0xFF;
        memcpy((cia->tmd).title_version, title_version_le, 2);
        memcpy((cia->ticket).ticket_version, title_version_le, 2);
    }

    // write the CIA stub (take #2)
    FindTitleKey((Ticket*)&(cia->ticket), title_id);
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0)) ||
        (install && (InstallCiaSystemData(cia, path_dest) != 0))) {
        free(cia);
        return 1;
    }

    free(cia);
    return 0;
}

u32 BuildInstallFromNdsFile(const char* path_nds, const char* path_dest, bool install) {
    TwlHeader twl;
    u8 title_id[8];
    u32 save_size = 0;
    u32 privsave_size = 0;
    u8 twl_flag = 0;

    // Init progress bar
    if (!ShowProgress(0, 0, path_nds)) return 1;

    // load TWL header, get save sizes, srl flag && title id
    if (fvx_qread(path_nds, &twl, 0, sizeof(TwlHeader), NULL) != FR_OK)
        return 1;
    for (u32 i = 0; i < 8; i++)
        title_id[i] = (twl.title_id >> ((7-i)*8)) & 0xFF;
    save_size = twl.pubsav_size;
    privsave_size = twl.prvsav_size;
    twl_flag = twl.srl_flag;

    // some basic sanity checks
    // see: https://problemkaputt.de/gbatek.htm#dsicartridgeheader
    // (gamecart dumps are not allowed)
    static const u8 tidhigh_dsiware[4] = { 0x00, 0x03, 0x00, 0x04 };
    if ((memcmp(title_id, tidhigh_dsiware, 3) != 0) || !title_id[3])
        return 1;

    // convert DSi title ID to 3DS title ID
    static const u8 tidhigh_3ds[4] = { 0x00, 0x04, 0x80, 0x04 };
    memcpy(title_id, tidhigh_3ds, 3);

    // build the CIA stub
    CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
    if (!cia) return 1;
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header), TICKET_COMMON_SIZE) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildFakeTicket((Ticket*)&(cia->ticket), title_id) != 0) ||
        (BuildFakeTmd(&(cia->tmd), title_id, 1, save_size, privsave_size, twl_flag)) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0))) {
        free(cia);
        return 1;
    }

    // insert / install NDS content
    TmdContentChunk* chunk = cia->content_list;
    memset(chunk, 0, sizeof(TmdContentChunk)); // nothing else to do
    if ((!install && (InsertCiaContent(path_dest, path_nds, 0, 0, chunk, NULL, false, false, false) != 0)) ||
        (install && (InstallCiaContent(path_dest, path_nds, 0, 0, chunk, title_id, NULL, false, false) != 0))) {
        free(cia);
        return 1;
    }

    // write the CIA stub (take #2)
    FindTitleKey((Ticket*)(&cia->ticket), title_id);
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (!install && (WriteCiaStub(cia, path_dest) != 0)) ||
        (install && (InstallCiaSystemData(cia, path_dest) != 0))) {
        free(cia);
        return 1;
    }

    free(cia);
    return 0;
}

u64 GetGameFileTitleId(const char* path) {
    u64 filetype = IdentifyFileType(path);
    u64 tid64 = 0;

    if (filetype & GAME_CIA) {
        CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
        if (!cia) return 0;
        if (LoadCiaStub(cia, path) == 0)
            tid64 = getbe64(cia->tmd.title_id);
        free(cia);
    } else if (filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD)) {
        u8 tid[8];
        if (fvx_qread(path, tid, 0x18C, 8, NULL) == FR_OK)
            tid64 = getbe64(tid);
    } else if (filetype & GAME_TICKET) {
        u8 tid[8];
        if (fvx_qread(path, tid, 0x1DC, 8, NULL) == FR_OK)
            tid64 = getbe64(tid);
    } else if (filetype & GAME_TAD) {
        TadHeader tad;
        if (fvx_qread(path, &tad, TAD_HEADER_OFFSET, sizeof(TadHeader), NULL) == FR_OK)
            tid64 = tad.title_id;
    } else if (filetype & GAME_NCCH) {
        NcchHeader ncch;
        if (LoadNcchHeaders(&ncch, NULL, NULL, path, 0) == 0)
            tid64 = ncch.partitionId;
    } else if (filetype & GAME_NCSD) {
        NcsdHeader ncsd;
        if (LoadNcsdHeader(&ncsd, path) == 0)
            tid64 = ncsd.mediaId;
    } else if (filetype & GAME_NDS) {
        TwlHeader twl;
        if ((fvx_qread(path, &twl, 0, 0x300, NULL) == FR_OK) && (twl.unit_code & 0x02))
            tid64 = twl.title_id;
    } else if (filetype & GAME_TIE) {
        if ((*path == 'T') && (sscanf(path, "T:/%016llx", &tid64) != 1))
            tid64 = 0;
    }

    if ((tid64 & 0xFFFFFF0000000000ull) == 0x0003000000000000ull)
        tid64 = 0x0004800000000000ull | (tid64 & 0xFFFFFFFFFFull);
    return tid64;
}

u32 GetGameFileTitleVersion(const char* path) {
    u64 filetype = IdentifyFileType(path);
    u32 version = (u32) -1;

    if (filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD)) {
        TitleMetaData tmd;
        if (fvx_qread(path, &tmd, 0, TMD_SIZE_STUB, NULL) == FR_OK)
            version = getbe16(tmd.title_version);
    } else if (filetype & GAME_TIE) {
        TitleInfoEntry tie;
        if (fvx_qread(path, &tie, 0, sizeof(TitleInfoEntry), NULL) == FR_OK)
            version = tie.title_version & 0xFFFF;
    } else if (filetype & GAME_CIA) {
        CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
        if (cia && LoadCiaStub(cia, path) == 0)
            version = getbe16(cia->tmd.title_version);
        if (cia) free(cia);
    }   

    return version;
}

u32 BuildCiaFromGameFile(const char* path, bool force_legit) {
    u64 filetype = IdentifyFileType(path);
    char dest[256];
    u32 ret = 0;

    // build output name
    snprintf(dest, sizeof(dest), OUTPUT_PATH "/");
    char* dname = dest + strnlen(dest, 256);
    if (!((filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD|GAME_TIE)) ||
        (strncmp(path + 1, ":/title/", 8) == 0)) ||
        (GetGoodName(dname, path, false) != 0)) {
        u64 title_id = (filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD)) ? GetGameFileTitleId(path) : 0;
        if (!title_id) {
            char* name = strrchr(path, '/');
            if (!name) return 1;
            snprintf(dest, sizeof(dest), "%s/%s", OUTPUT_PATH, ++name);
        } else snprintf(dest, sizeof(dest), "%s/%016llX", OUTPUT_PATH, title_id);
    }
    // replace extension
    char* dot = strrchr(dest, '.');
    if (!dot || (strpbrk(dot, "/(){}[]!$#*+-")))
        dot = dest + strnlen(dest, 256);
    snprintf(dot, 16, ".%s", "tmp.cia");

    if (!CheckWritePermissions(dest)) return 1;
    f_unlink(dest); // remove the file if it already exists

    // ensure the output dir exists
    if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
        return 1;

    // build CIA from game file
    if (filetype & GAME_TIE)
        ret = BuildCiaFromTieFile(path, dest, force_legit);
    else if (filetype & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD))
        ret = BuildCiaFromTmdFile(path, dest, force_legit, filetype & (GAME_CDNTMD|GAME_TWLTMD));
    else if (filetype & GAME_NCCH)
        ret = BuildInstallFromNcchFile(path, dest, false);
    else if (filetype & GAME_NCSD)
        ret = BuildInstallFromNcsdFile(path, dest, false);
    else if ((filetype & GAME_NDS) && (filetype & FLAG_DSIW))
        ret = BuildInstallFromNdsFile(path, dest, false);
    else if (filetype & GAME_TAD)
        ret = BuildCiaFromTadFile(path, dest, force_legit);
    else ret = 1;

    // finalizing CIA build...
    if (ret != 0) { // try to get rid of the borked file
        fvx_unlink(dest);
    } else { // find a proper extension for CIA
        char dest_old[256];
        strncpy(dest_old, dest, 256);
        CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
        if (!cia) return 1;
        if (LoadCiaStub(cia, dest) != 0) {
            free(cia);
            return 1;
        }

        if ((ValidateTmdSignature(&(cia->tmd)) == 0) && (VerifyTmd(&(cia->tmd)) == 0)) {
            Ticket* ticket = (Ticket*) &(cia->ticket);
            if (ValidateTicketSignature(ticket) == 0)
                snprintf(dot, 32, ".%08lX.%s", getbe32(ticket->console_id), "legit.cia");
            else snprintf(dot, 32, ".%s", "piratelegit.cia");
        } else snprintf(dot, 16, ".%s", "standard.cia");
        free(cia);

        fvx_unlink(dest);
        fvx_rename(dest_old, dest);
    }

    return ret;
}

u32 InstallGameFile(const char* path, bool to_emunand) {
    char drv[3];
    u64 filetype = IdentifyFileType(path);
    u32 ret = 0;

    // find out the destination
    u64 tid64 = GetGameFileTitleId(path);
    if (GetInstallDataDrive(drv, tid64, to_emunand) != 0)
        return 1;

    // check dbs
    char path_db[32];
    if (((GetInstallDbsPath(path_db, drv, "title.db" ) != 0) || !fvx_qsize(path_db)) ||
        ((GetInstallDbsPath(path_db, drv, "import.db") != 0) || !fvx_qsize(path_db)) ||
        ((GetInstallDbsPath(path_db, drv, "ticket.db") != 0) || !fvx_qsize(path_db))) {
        ShowPrompt(false, "%s", STR_INSTALL_ERROR_THIS_SYSTEM_IS_MISSING_DB_FILES_MAYBE_SD_MISSING_OR_UNINITIALIZED);
        return 1;
    }

    // check permissions for SysNAND (this includes everything we need)
    if (!CheckWritePermissions(to_emunand ? "4:" : "1:")) return 1;

    // cleanup content folder before starting install
    ShowProgress(0, 0, path);
    UninstallGameData(tid64, false, false, false, to_emunand);

    // install game file
    if (filetype & GAME_CIA)
        ret = InstallFromCiaFile(path, drv);
    else if (filetype & (GAME_CDNTMD|GAME_TWLTMD))
        ret = InstallFromTmdFile(path, drv);
    else if (filetype & GAME_NCCH)
        ret = BuildInstallFromNcchFile(path, drv, true);
    else if (filetype & GAME_NCSD)
        ret = BuildInstallFromNcsdFile(path, drv, true);
    else if ((filetype & GAME_NDS) && (filetype & FLAG_DSIW))
        ret = BuildInstallFromNdsFile(path, drv, true);
    else ret = 1;

    // cleanup on failed installs, but leave ticket and save untouched
    if (ret != 0) UninstallGameData(tid64, true, false, false, to_emunand);

    return ret;
}

u32 InstallCifinishFile(const char* path, bool to_emunand) {
    u8 ALIGN(4) seeddb_storage[sizeof(SeedInfo) + sizeof(SeedInfoEntry)];
    SeedInfo* seeddb = (SeedInfo*) (void*) seeddb_storage;
    TicketCommon ticket;
    u32 ret = 0;

    // sanity check / preparations
    if (!(IdentifyFileType(path) & BIN_CIFNSH)) return 1;
    if (BuildFakeTicket((Ticket*) &ticket, NULL) != 0) return 1;
    seeddb->n_entries = 1;

    // check ticket db
    char path_ticketdb[32];
    if ((GetInstallDbsPath(path_ticketdb, to_emunand ? "4:" : "1:", "ticket.db") != 0) || !fvx_qsize(path_ticketdb)) {
        ShowPrompt(false, "%s", STR_INSTALL_ERROR_THIS_SYSTEM_IS_MISSING_TICKET_DB);
        return 1;
    }

    // check permissions for SysNAND (this includes everything we need)
    if (!CheckWritePermissions(to_emunand ? "4:" : "1:")) return 1;

    // store mount path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;

    // load the entire cifinish file into memory
    CifinishHeader* cifinish = (CifinishHeader*) malloc(fvx_qsize(path));
    CifinishTitle* cftitle = (CifinishTitle*) (cifinish+1);
    if (!cifinish) return 1;
    if (fvx_qread(path, cifinish, 0, fvx_qsize(path), NULL) != FR_OK) {
        free(cifinish);
        return 1;
    }

    // process tickets for the entire cifinish file
    if (!ShowProgress(0, 0, path)) ret = 1;
    if (!InitImgFS(path_ticketdb)) ret = 1;
    for (u32 i = 0; !ret && (i < cifinish->n_entries); i++) {
        // sanity
        if (strncmp(cftitle[i].magic, CIFINISH_TITLE_MAGIC, strlen(CIFINISH_TITLE_MAGIC)) != 0) {
            ret = 1;
            continue;
        }
        // check for forbidden title id (the "too large dlc")
        if ((TITLE_MAX_CONTENTS <= 1024) && (cftitle[i].title_id == 0x0004008C000CBD00)) {
            ShowPrompt(false, "%s", STR_SKIPPED_TITLE_0004008C000CBD00_NEEDS_SPECIAL_COMPILE_FLAGS);
            ShowProgress(0, 0, path);
            continue;
        }
        if (!ShowProgress(i, cifinish->n_entries, path)) ret = 1;
        // insert ticket with correct title id
        for (u32 t = 0; t < 8; t++)
            ticket.title_id[7-t] = (cftitle[i].title_id >> (8*t)) & 0xFF;
        AddTicketToDB(PART_PATH, ticket.title_id, (Ticket*) &ticket, false);
    }

    // process seeds for the entire cifinish file
    if (!ShowProgress(0, 0, path)) ret = 1;
    for (u32 i = 0; !ret && (i < cifinish->n_entries); i++) {
        if (!ShowProgress(i, cifinish->n_entries, path)) ret = 1;
        if ((TITLE_MAX_CONTENTS <= 1024) && (cftitle[i].title_id == 0x0004008C000CBD00)) continue;
        if (!cftitle[i].has_seed) continue;
        seeddb->entries[0].titleId = cftitle[i].title_id;
        memcpy(&(seeddb->entries[0].seed), cftitle[i].seed, sizeof(Seed));
        ret = InstallSeedDbToSystem(seeddb, to_emunand);
    }

    // cleanup
    InitImgFS(path_bak);
    free(cifinish);

    return ret;
}

u32 InstallTicketFile(const char* path, bool to_emunand) {
    // sanity check
    if (!(IdentifyFileType(path) & GAME_TICKET))
        return 1;

    // path string
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // check ticket db
    char path_ticketdb[32];
    if ((GetInstallDbsPath(path_ticketdb, to_emunand ? "4:" : "1:", "ticket.db") != 0) || !fvx_qsize(path_ticketdb)) {
        ShowPrompt(false, "%s", STR_INSTALL_ERROR_THIS_SYSTEM_IS_MISSING_TICKET_DB);
        return 1;
    }

    // load & verify ticket
    Ticket* ticket;
    if (LoadTicketFile(&ticket, path) != 0)
        return 1;
    if (ValidateTicketSignature(ticket) != 0) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_FAKE_SIGNED_TICKET_ONLY_VALID_SIGNED_TICKETS_CAN_BE_INSTALLED);
        free(ticket);
        return 1;
    }

    // check ticket console id
    u32 cid = getbe32(ticket->console_id);
    if (cid && (cid != (&ARM9_ITCM->otp)->deviceId)) {
        ShowPrompt(false, STR_PATH_ERROR_UNKNOWN_CID_N_THIS_TICKET_DOES_NOT_BELONG_TO_THIS_3DS, pathstr, cid);
        free(ticket);
        return 1;
    }

    // check permissions for SysNAND
    if (!CheckWritePermissions(to_emunand ? "4:" : "1:")) {
        free(ticket);
        return 1;
    }

    // let the user know we're working
    ShowString("%s\n%s\n", pathstr, STR_INSTALLING_TICKET);

    // write ticket database
    // ensure remounting the old mount path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;

    // ticket database
    if (!InitImgFS(path_ticketdb) ||
        ((AddTicketToDB(PART_PATH, ticket->title_id, (Ticket*) ticket, true)) != 0)) {
        InitImgFS(path_bak);
        free(ticket);
        return 1;
    }

    // restore old mount path
    InitImgFS(path_bak);

    free(ticket);
    return 0;
}

u32 DumpTicketForGameFile(const char* path, bool force_legit) {
    u64 tid64 = GetGameFileTitleId(path);
    if (!tid64) return 1;

    Ticket* ticket;
    if (LoadTicketForTitleId(&ticket, tid64) != 0)
        return 1;

    if ((ValidateTicket(ticket) != 0) ||
        (force_legit && (ValidateTicketSignature(ticket) != 0))) {
        free(ticket);
        return 1;
    }
    
    // build output name
    char dest[256];
    snprintf(dest, sizeof(dest), OUTPUT_PATH "/");
    char* dname = dest + strnlen(dest, 256);
    if (GetGoodName(dname, path, false) != 0)
        snprintf(dest, sizeof(dest), "%s/%016llX", OUTPUT_PATH, tid64);

    // replace extension
    char* dot = strrchr(dest, '.');
    if (!dot || (strpbrk(dot, "/(){}[]!$#*+-")))
        dot = dest + strnlen(dest, 256);
    snprintf(dot, 16, ".%s", force_legit ? "legit.tik" : "tik");

    // dump ticket
    if (!CheckWritePermissions(dest)) return 1;
    f_unlink(dest); // remove the file if it already exists
    fvx_qwrite(dest, ticket, 0, GetTicketSize(ticket), NULL);

    free(ticket);
    return 0;
}

// this has very limited uses right now
u32 DumpCxiSrlFromTmdFile(const char* path) {
    u64 filetype = 0;
    char path_cxi[256];
    char dest[256];

    // prepare output name
    snprintf(dest, sizeof(dest), OUTPUT_PATH "/");
    char* dname = dest + strnlen(dest, 256);
    if (!CheckWritePermissions(dest)) return 1;

    // ensure the output dir exists
    if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
        return 1;

    // get path to CXI/SRL and decrypt (if encrypted)
    if ((strncmp(path + 1, ":/title/", 8) != 0) ||
        (GetTmdContentPath(path_cxi, path) != 0) ||
        (!((filetype = IdentifyFileType(path_cxi)) & (GAME_NCCH|GAME_NDS))) ||
        (GetGoodName(dname, path_cxi, false) != 0) ||
        (CryptNcchNcsdBossFirmFile(path_cxi, dest, filetype, CRYPTO_DECRYPT, 0, 0, NULL, NULL) != 0)) {
        if (*dname) fvx_unlink(dest);
        return 1;
    }

    return 0;
}

u32 DumpCxiSrlFromGameFile(const char* path) {
    u64 filetype = IdentifyFileType(path);
    const char* path_tmd = path;
    char path_data[256];
    if (!(filetype & (GAME_TIE|GAME_TMD)))
        return 1;

    if (filetype & GAME_TIE) {
        if (GetTieTmdPath(path_data, path) != 0)
            return 1;
        path_tmd = path_data;
    }
    
    return DumpCxiSrlFromTmdFile(path_tmd);
}

u32 ExtractCodeFromCxiFile(const char* path, const char* path_out, char* extstr) {
    u64 filetype = IdentifyFileType(path);
    char dest[256];
    if (!path_out && (fvx_rmkdir(OUTPUT_PATH) != FR_OK)) return 1;
    strncpy(dest, path_out ? path_out : OUTPUT_PATH, 256);
    dest[255] = '\0';
    if (!CheckWritePermissions(dest)) return 1;

    // extstr should be at least 16 bytes in size

    // NCSD handling
    u32 ncch_offset = 0;
    if (filetype & GAME_NCSD) {
        NcsdHeader ncsd;
        if (LoadNcsdHeader(&ncsd, path) == 0)
            ncch_offset = ncsd.partitions[0].offset * NCSD_MEDIA_UNIT;
        else return 1;
    }

    // load all required headers
    NcchHeader ncch;
    NcchExtHeader exthdr;
    ExeFsHeader exefs;
    if (LoadNcchHeaders(&ncch, &exthdr, &exefs, path, ncch_offset) != 0) return 1;

    // find ".code" or ".firm" inside the ExeFS header
    u32 code_size = 0;
    u32 code_offset = 0;
    for (u32 i = 0; i < 10; i++) {
        if (exefs.files[i].size &&
            ((strncmp(exefs.files[i].name, EXEFS_CODE_NAME, 8) == 0) ||
             (strncmp(exefs.files[i].name, ".firm", 8) == 0))) {
            code_size = exefs.files[i].size;
            code_offset = (ncch.offset_exefs * NCCH_MEDIA_UNIT) + sizeof(ExeFsHeader) + exefs.files[i].offset;
        }
    }

    // if code is compressed: find decompressed size
    u32 code_max_size = code_size;
    if (exthdr.flag & 0x1) {
        u8 footer[8];
        if (code_size < 8) return 1;
        if ((fvx_qread(path, footer, ncch_offset + code_offset + code_size - 8, 8, NULL) != FR_OK) ||
            (DecryptNcch(footer, code_offset + code_size - 8, 8, &ncch, &exefs) != 0))
            return 1;
        u32 unc_size = GetCodeLzssUncompressedSize(footer, code_size);
        code_max_size = max(code_size, unc_size);
    }

    // allocate memory
    u8* code = (u8*) malloc(code_max_size);
    if (!code) {
        ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
        return 1;
    }

    // load .code
    if ((fvx_qread(path, code, ncch_offset + code_offset, code_size, NULL) != FR_OK) ||
        (DecryptNcch(code, code_offset, code_size, &ncch, &exefs) != 0)) {
        free(code);
        return 1;
    }

    // decompress code (only if required)
    if ((exthdr.flag & 0x1) && (DecompressCodeLzss(code, &code_size, code_max_size) != 0)) {
        free(code);
        return 1;
    }

    // finalize output path (if not already final)
    char* ext = EXEFS_CODE_NAME;
    if (code_size >= 0x200) {
        if (ValidateFirmHeader((FirmHeader*)(void*) code, code_size) == 0) ext = ".firm";
        else if (ValidateAgbHeader((AgbHeader*)(void*) code) == 0) ext = ".gba";
    }
    if (extstr) strncpy(extstr, ext, 7);
    if (!path_out) snprintf(dest, sizeof(dest), OUTPUT_PATH "/%016llX%s%s", ncch.programId, (exthdr.flag & 0x1) ? ".dec" : "", ext);

    // write output file
    fvx_unlink(dest);
    if (fvx_qwrite(dest, code, 0, code_size, NULL) != FR_OK) {
        fvx_unlink(dest);
        free(code);
        return 1;
    }

    free(code);
    return 0;
}

u32 CompressCode(const char* path, const char* path_out) {
    char dest[256];

    strncpy(dest, path_out ? path_out : OUTPUT_PATH, 255);
    if (!CheckWritePermissions(dest)) return 1;
    if (!path_out && (fvx_rmkdir(OUTPUT_PATH) != FR_OK)) return 1;

    // allocate memory
    u32 code_dec_size = fvx_qsize(path);
    u8* code_dec = (u8*) malloc(code_dec_size);
    u32 code_cmp_size = code_dec_size;
    u8* code_cmp = (u8*) malloc(code_cmp_size);
    if (!code_dec || !code_cmp) {
        if (code_dec != NULL) free(code_dec);
        if (code_cmp != NULL) free(code_cmp);
        ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
        return 1;
    }

    // load code.bin and compress code
    if ((fvx_qread(path, code_dec, 0, code_dec_size, NULL) != FR_OK) ||
        (!CompressCodeLzss(code_dec, code_dec_size, code_cmp, &code_cmp_size))) {
        free(code_dec);
        free(code_cmp);
        return 1;
    }

    // write output file
    fvx_unlink(dest);
    free(code_dec);
    if (fvx_qwrite(dest, code_cmp, 0, code_cmp_size, NULL) != FR_OK) {
        fvx_unlink(dest);
        free(code_cmp);
        return 1;
    }

    free(code_cmp);
    return 0;
}

u64 GetAnyFileTrimmedSize(const char* path) {
    u64 fsize = 0;
    u64 trimsize = 0;
    u8 pad_byte = 0x7F; 
    FIL fp;
    UINT br;

    if (fvx_open(&fp, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 0;

    fsize = fvx_size(&fp);
    u32 bufsize = min(STD_BUFFER_SIZE, fsize);
    u8* buffer = (u8*) malloc(bufsize);
    if (!buffer) return 0;

    for (s64 pos = align(fsize, bufsize) - bufsize; (pos >= 0) && !trimsize; pos -= bufsize) {
        if ((fvx_lseek(&fp, (UINT) pos) != FR_OK) ||
            (fvx_read(&fp, buffer, bufsize, &br) != FR_OK) || !br) break;
        if (pad_byte == 0x7F) { // start value
            pad_byte = buffer[br-1];
            if ((pad_byte != 0x00) && (pad_byte != 0xFF)) break;
        }
        for (u8* b = buffer + (br-1); b >= buffer; b--) {
            if (*b != pad_byte) {
                trimsize = pos + (b-buffer) + 1;
                break;
            }
        }
    }

    fvx_close(&fp);
    free(buffer);

    // 4 byte forced alignment
    // 512 byte trimming minimum
    trimsize = align(trimsize, 4);
    if ((trimsize > fsize) || (fsize - trimsize < 0x200)) return 0;
    return trimsize;
}

u64 GetGameFileTrimmedSize(const char* path) {
    u64 filetype = IdentifyFileType(path);
    u64 trimsize = 0;

    if (filetype & GAME_GBA) {
        trimsize = GetAnyFileTrimmedSize(path);
    } else if (filetype & GAME_NDS) {
        TwlHeader hdr;
        if (fvx_qread(path, &hdr, 0, sizeof(TwlHeader), NULL) != FR_OK) {
            return 0;
        } if (hdr.unit_code != 0x00) { // DSi or NDS+DSi
            trimsize = hdr.ntr_twl_rom_size;
        } else if (hdr.ntr_rom_size) { // regular NDS
            trimsize = hdr.ntr_rom_size;

            // Check if immediately after the reported cart size
            // is the magic number string 'ac' (auth code).
            // If found, add 0x88 bytes for the download play RSA key.
            u16 rsaMagic;
            if(fvx_qread(path, &rsaMagic, trimsize, 2, NULL) == FR_OK && rsaMagic == 0x6361) {
                trimsize += 0x88;
            }
        }
    } else {
        u8 hdr[0x200];
        if (fvx_qread(path, &hdr, 0, 0x200, NULL) != FR_OK)
            return 0;
        if (filetype & IMG_NAND)
            trimsize = GetNandNcsdMinSizeSectors((NandNcsdHeader*) (void*) hdr) * 0x200;
        else if (filetype & SYS_FIRM)
            trimsize = GetFirmSize((FirmHeader*) (void*) hdr);
        else if (filetype & GAME_NCSD)
            trimsize = GetNcsdTrimmedSize((NcsdHeader*) (void*) hdr);
        else if (filetype & GAME_NCCH)
            trimsize = ((NcchHeader*) (void*) hdr)->size * NCCH_MEDIA_UNIT;
    }

    // safety check for file size
    if (trimsize > fvx_qsize(path))
        trimsize = 0;

    return trimsize;
}

u32 TrimGameFile(const char* path) {
    u64 trimsize = GetGameFileTrimmedSize(path);
    if (!trimsize) return 1;

    // actual truncate routine - FAT only
    FIL fp;
    if (fx_open(&fp, path, FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    if ((f_lseek(&fp, (u32) trimsize) != FR_OK) || (f_truncate(&fp) != FR_OK)) {
        fx_close(&fp);
        return 1;
    }
    fx_close(&fp);

    // all done
    return 0;
}

u32 LoadSmdhFromGameFile(const char* path, Smdh* smdh) {
    u64 filetype = IdentifyFileType(path);

    if (filetype & GAME_SMDH) { // SMDH file
        UINT btr;
        if ((fvx_qread(path, smdh, 0, sizeof(Smdh), &btr) == FR_OK) || (btr == sizeof(Smdh))) return 0;
    } else if (filetype & GAME_NCCH) { // NCCH file
        if (LoadExeFsFile(smdh, path, 0, "icon", sizeof(Smdh), NULL) == 0) return 0;
    } else if (filetype & GAME_NCSD) { // NCSD file
        if (LoadExeFsFile(smdh, path, NCSD_CNT0_OFFSET, "icon", sizeof(Smdh), NULL) == 0) return 0;
    } else if (filetype & GAME_CIA) { // CIA file
        CiaInfo info;
        if ((fvx_qread(path, &info, 0, 0x20, NULL) != FR_OK) ||
            (GetCiaInfo(&info, (CiaHeader*) &info) != 0)) return 1;
        if ((info.offset_meta) && (fvx_qread(path, smdh, info.offset_meta + 0x400, sizeof(Smdh), NULL) == FR_OK)) return 0;
        else if (LoadExeFsFile(smdh, path, info.offset_content, "icon", sizeof(Smdh), NULL) == 0) return 0;
    } else if (filetype & GAME_TMD) {
        char path_content[256];
        if (GetTmdContentPath(path_content, path) != 0) return 1;
        return LoadSmdhFromGameFile(path_content, smdh);
    } else if (filetype & GAME_TIE) {
        char path_content[256];
        if (GetTieContentPath(path_content, path) != 0) return 1;
        return LoadSmdhFromGameFile(path_content, smdh);
    } else if (filetype & GAME_TICKET) {
        char path_content[256];
        if (GetTicketContentPath(path_content, path) != 0) return 1;
        return LoadSmdhFromGameFile(path_content, smdh);
    } else if (filetype & GAME_3DSX) {
        ThreedsxHeader threedsx;
        if ((fvx_qread(path, &threedsx, 0, sizeof(ThreedsxHeader), NULL) != FR_OK) ||
            (!threedsx.offset_smdh || (threedsx.size_smdh != sizeof(Smdh))) ||
            (fvx_qread(path, smdh, threedsx.offset_smdh, sizeof(Smdh), NULL) != FR_OK))
            return 1;
        return 0;
    }

    return 1;
}

u32 ShowSmdhTitleInfo(Smdh* smdh, u16* screen) {
    static const u8 smdh_magic[] = { SMDH_MAGIC };
    const u32 lwrap = 24;
    u16 icon[SMDH_SIZE_ICON_BIG / sizeof(u16)];
    char desc_l[SMDH_SIZE_DESC_LONG+1];
    char desc_s[SMDH_SIZE_DESC_SHORT+1];
    char pub[SMDH_SIZE_PUBLISHER+1];
    if ((memcmp(smdh->magic, smdh_magic, 4) != 0) ||
        (GetSmdhIconBig(icon, smdh) != 0) ||
        (GetSmdhDescLong(desc_l, smdh) != 0) ||
        (GetSmdhDescShort(desc_s, smdh) != 0) ||
        (GetSmdhPublisher(pub, smdh) != 0))
        return 1;
    WordWrapString(desc_l, lwrap);
    WordWrapString(desc_s, lwrap);
    WordWrapString(pub, lwrap);
    ShowIconStringF(screen, icon, SMDH_DIM_ICON_BIG, SMDH_DIM_ICON_BIG, "%s\n%s\n%s", desc_l, desc_s, pub);
    return 0;
}

u32 ShowTwlIconTitleInfo(TwlIconData* twl_icon, u16* screen) {
    const u32 lwrap = 24;
    u16 icon[TWLICON_SIZE_ICON / sizeof(u16)];
    char desc[TWLICON_SIZE_DESC+1];
    if ((GetTwlIcon(icon, twl_icon) != 0) ||
        (GetTwlTitle(desc, twl_icon) != 0))
        return 1;
    WordWrapString(desc, lwrap);
    ShowIconStringF(screen, icon, TWLICON_DIM_ICON, TWLICON_DIM_ICON, "%s", desc);
    return 0;
}

u32 ShowGbaFileTitleInfo(const char* path, u16* screen) {
    AgbHeader agb;
    if ((fvx_qread(path, &agb, 0, sizeof(AgbHeader), NULL) != FR_OK) ||
        (ValidateAgbHeader(&agb) != 0)) return 1;
    ClearScreen(screen, COLOR_STD_BG);
    ShowStringF(screen, "%.12s (AGB-%.4s)\n%s", agb.game_title, agb.game_code, AgbDestStr(agb.game_code));
    return 0;
}

u32 ShowGameFileIcon(const char* path, u16* screen) {
    char path_content[256];
    u64 itype = IdentifyFileType(path); // initial type
    if (itype & GAME_TMD) {
        if (GetTmdContentPath(path_content, path) != 0) return 1;
        path = path_content;
    } else if (itype & GAME_TIE) {
         if (GetTieContentPath(path_content, path) != 0) return 1;
        path = path_content;
    } else if (itype & GAME_TICKET) {
         if (GetTicketContentPath(path_content, path) != 0) return 1;
        path = path_content;
    }

    void* buffer = (void*) malloc(max(sizeof(Smdh), sizeof(TwlIconData)));
    Smdh* smdh = (Smdh*) buffer;
    TwlIconData* twl_icon = (TwlIconData*) buffer;

    // try loading SMDH, then try NDS / encrypted / GBA
    u32 ret = 1;
    u32 tp = 0;
    if (LoadSmdhFromGameFile(path, smdh) == 0)
        ret = ShowSmdhTitleInfo(smdh, screen);
    else if ((LoadTwlMetaData(path, NULL, twl_icon) == 0) ||
        ((itype & GAME_TAD) && (fvx_qread(path, twl_icon, TAD_BANNER_OFFSET, sizeof(TwlIconData), NULL) == FR_OK)))
        ret = ShowTwlIconTitleInfo(twl_icon, screen);
    else if ((tp = LoadEncryptedIconFromCiaTmd(path, buffer, NULL, false)) != 0)
        ret = (tp == GAME_NCCH) ? ShowSmdhTitleInfo(smdh, screen) :
              (tp == GAME_NDS ) ? ShowTwlIconTitleInfo(twl_icon, screen) : 1;
    else ret = ShowGbaFileTitleInfo(path, screen);

    free(buffer);
    return ret;
}

u32 ShowGameCheckerInfo(const char* path) {
    u64 type = IdentifyFileType(path); // filetype
    if (!(type & (GAME_CIA|GAME_TIE|GAME_TMD|GAME_CDNTMD|GAME_TWLTMD)))
        return 1;

    u32 content_count = 0;
    u32 content_found = 0;
    bool sd_title = false;
    bool missing_first = false;

    Ticket* ticket = NULL;
    TitleMetaData* tmd = (TitleMetaData*) malloc(TMD_SIZE_MAX);
    if (!tmd) return 1;

    // path string
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // CIA / TIE specific stuff
    if (type & GAME_TIE) {
        // load TMD
        char path_tmd[256];
        if ((GetTieTmdPath(path_tmd, path) != 0) ||
            (LoadTmdFile(tmd, path_tmd) != 0)) {
            free(tmd);
            tmd = NULL;
        } else {
            content_found = content_count = getbe16(tmd->content_count);
            sd_title = (*path_tmd == 'A') || (*path_tmd == 'B');
        }
        
        // load ticket
        u64 tid64 = tmd ? getbe64(tmd->title_id) : 0;
        if (LoadTicketForTitleId(&ticket, tid64) != 0)
            ticket = NULL;
    } else if (type & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD)) {
        if (LoadTmdFile(tmd, path) != 0) {
            free(tmd);
            tmd = NULL;
        } else content_found = content_count = getbe16(tmd->content_count);
    } else if (type & GAME_CIA) {
        CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
        if (!cia) {
            free(tmd);
            return 1;
        }

        // load CIA stub
        if (LoadCiaStub(cia, path) != 0) {
            ShowPrompt(false, "%s\n%s", pathstr, STR_ERROR_PROBABLY_NOT_CIA_FILE);
            free(cia);
            free(tmd);
            return 1;
        }

        // check for available contents
        u8* cnt_index = cia->header.content_index;
        content_count = getbe16(cia->tmd.content_count);
        for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
            TmdContentChunk* chunk = &(cia->content_list[i]);
            u16 index = getbe16(chunk->index);
            if (cnt_index[index/8] & (1 << (7-(index%8)))) content_found++;
            else if (i == 0) missing_first = true;
        }

        // copy TMD, ticket
        memcpy(tmd, &(cia->tmd), cia->header.size_tmd);
        ticket = (Ticket*) malloc(cia->header.size_ticket);
        if (ticket) memcpy(ticket, &(cia->ticket), cia->header.size_ticket);
        
        free(cia);
    }

    // states: 0 -> invalid / 1 -> valid / badsig / 2 -> valid / goodsig
    u32 state_ticket = 0;
    u32 state_tmd = 0;
    u64 title_id = 0;
    u32 console_id = 0;
    u32 title_version = 0;
    u64 content_size = 0;
    bool is_dlc = false;

    // basic info
    if (tmd) title_id = getbe64(tmd->title_id);
    if (tmd) title_version = getbe16(tmd->title_version);
    if (ticket) console_id = getbe32(ticket->console_id);
    is_dlc = ((title_id >> 32) == 0x0004008C);

    // size of contents
    char bytestr[32];
    if (tmd && content_count) {
        TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
        for (u32 i = 0; i < content_count; i++, chunk++)
            content_size += getbe64(chunk->size);
    }
    FormatBytes(bytestr, content_size);

    // check ticket
    if (ticket && ValidateTicket(ticket) == 0)
        state_ticket = (ValidateTicketSignature(ticket) == 0) ? 2 : 1;

    // check tmd
    if (tmd && VerifyTmd(tmd) == 0)
        state_tmd = (ValidateTmdSignature(tmd) == 0) ? 2 : 1;

    // CIA / title type string
    const char *typestr;
    if ((!state_ticket && (type&(GAME_CIA|GAME_TIE))) || !state_tmd || missing_first ||
        (!is_dlc && (content_found != content_count)))
        typestr = STR_POSSIBLY_BROKEN;
    else {
        if (console_id) {
            if (state_tmd == 2) {
                if (state_ticket == 2) typestr = is_dlc ? STR_PERSONAL_LEGIT_DLC : STR_PERSONAL_LEGIT;
                else typestr = is_dlc ? STR_PERSONAL_PIRATE_LEGIT_DLC : STR_PERSONAL_PIRATE_LEGIT;
            } else typestr = is_dlc ? STR_PERSONAL_CUSTOM_DLC : STR_PERSONAL_CUSTOM;
        } else {
            if (state_tmd == 2) {
                if (state_ticket == 2) typestr = is_dlc ? STR_UNIVERSAL_LEGIT_DLC : STR_UNIVERSAL_LEGIT;
                else typestr = is_dlc ? STR_UNIVERSAL_PIRATE_LEGIT_DLC : STR_UNIVERSAL_PIRATE_LEGIT;
            } else typestr = is_dlc ? STR_UNIVERSAL_CUSTOM_DLC : STR_UNIVERSAL_CUSTOM;
        }
    }

    char srcstr[5];
    snprintf(srcstr, sizeof(srcstr), "%s",
        (type & GAME_TIE) ? (tmd ? (sd_title ? "SD" : "NAND") : "UNK") :
        (type & GAME_CIA) ? "CIA" :
        (type & GAME_TMD) ? "TMD" :
        (type & GAME_CDNTMD) ? "CDN" :
        (type & GAME_TWLTMD) ? "TWL" : "UNK");

    char contents_str[64];
    if (type & GAME_CIA) snprintf(contents_str, sizeof(contents_str), STR_CONTENTS_IN_CIA_FOUND_TOTAL, content_found, content_count);
    else snprintf(contents_str, sizeof(contents_str), STR_CONTENTS_IN_CIA_TOTAL, content_count);

    char conid_str[32] = { '\0' };
    if (type & (GAME_CIA|GAME_TIE)) snprintf(conid_str, sizeof(conid_str), STR_CONSOLE_ID_N, console_id);


    // output results
    s32 state_verify = -1;
    while (true) {
        if (!ShowPrompt(state_verify < 0, STR_SHOW_GAME_INFO_DETAILS,
            pathstr, typestr, srcstr, title_id,
            (title_version>>10)&0x3F, (title_version>>4)&0x3F, (title_version)&0xF,
            bytestr, contents_str, conid_str,
            (state_ticket == 0) ? STR_STATE_UNKNOWN : (state_ticket == 2) ? STR_STATE_LEGIT : STR_STATE_ILLEGIT,
            (state_tmd == 0) ? STR_STATE_INVALID : (state_tmd == 2) ? STR_STATE_LEGIT : STR_STATE_ILLEGIT,
            (state_verify < 0) ? STR_STATE_PENDING_PROCEED_WITH_VERIFICATION : (state_verify == 0) ? STR_STATE_PASSED : STR_STATE_FAILED) ||
            (state_verify >= 0)) break;
        state_verify = VerifyGameFile(path);
    }

    if (tmd) free(tmd);
    if (ticket) free(ticket);
    return 0;
}

u32 BuildNcchInfoXorpads(const char* destdir, const char* path) {
    FIL fp_info;
    FIL fp_xorpad;
    UINT bt;

    if (!CheckWritePermissions(destdir)) return 1;
    // warning: this will only build output dirs in the root dir (!)
    if ((f_stat(destdir, NULL) != FR_OK) && (f_mkdir(destdir) != FR_OK))
        return 1;

    NcchInfoHeader info;
    u32 version = 0;
    u32 entry_size = 0;
    u32 ret = 0;
    if (fvx_open(&fp_info, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&fp_info, 0);
    if ((fvx_read(&fp_info, &info, sizeof(NcchInfoHeader), &bt) != FR_OK) ||
        (bt != sizeof(NcchInfoHeader))) {
        fvx_close(&fp_info);
        return 1;
    }
    version = GetNcchInfoVersion(&info);
    entry_size = (version == 3) ? NCCHINFO_V3_SIZE : sizeof(NcchInfoEntry);
    if (!version) ret = 1;

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) ret = 1;
    for (u32 i = 0; (i < info.n_entries) && (ret == 0); i++) {
        NcchInfoEntry entry;
        if ((fvx_read(&fp_info, &entry, entry_size, &bt) != FR_OK) ||
            (bt != entry_size)) ret = 1;
        if (FixNcchInfoEntry(&entry, version) != 0) ret = 1;
        if (ret != 0) break;

        char dest[256]; // 256 is the maximum length of a full path
        snprintf(dest, sizeof(dest), "%s/%s", destdir, entry.filename);
        if (fvx_open(&fp_xorpad, dest, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            if (!ShowProgress(0, 0, entry.filename)) ret = 1;
            for (u64 p = 0; (p < entry.size_b) && (ret == 0); p += STD_BUFFER_SIZE) {
                UINT create_bytes = min(STD_BUFFER_SIZE, entry.size_b - p);
                if (BuildNcchInfoXorpad(buffer, &entry, create_bytes, p) != 0) ret = 1;
                if (fvx_write(&fp_xorpad, buffer, create_bytes, &bt) != FR_OK) ret = 1;
                if (!ShowProgress(p + create_bytes, entry.size_b, entry.filename)) ret = 1;
            }
            fvx_close(&fp_xorpad);
        } else ret = 1;
        if (ret != 0) f_unlink(dest); // get rid of the borked file
    }

    if (buffer) free(buffer);
    fvx_close(&fp_info);
    return ret;
}

u32 GetHealthAndSafetyPaths(const char* drv, char* path_cxi, char* path_bak) {
    static const u32 tidlow_hs_o3ds[] = { 0x00020300, 0x00021300, 0x00022300, 0, 0x00026300, 0x00027300, 0x00028300 };
    static const u32 tidlow_hs_n3ds[] = { 0x20020300, 0x20021300, 0x20022300, 0, 0, 0x20027300, 0 };

    // get H&S title id low
    u32 tidlow_hs = 0;
    for (char secchar = 'C'; secchar >= 'A'; secchar--) {
        char path_secinfo[32];
        u8 secinfo[0x111];
        u32 region = 0xFF;
        UINT br;
        snprintf(path_secinfo, sizeof(path_secinfo), "%s/rw/sys/SecureInfo_%c", drv, secchar);
        if ((fvx_qread(path_secinfo, secinfo, 0, 0x111, &br) != FR_OK) ||
            (br != 0x111))
            continue;
        region = secinfo[0x100];
        if (region >= sizeof(tidlow_hs_o3ds) / sizeof(u32)) continue;
        tidlow_hs = (IS_O3DS) ?
            tidlow_hs_o3ds[region] : tidlow_hs_n3ds[region];
        break;
    }
    if (!tidlow_hs) return 1;

    // build paths
    if (path_cxi) *path_cxi = '\0';
    if (path_bak) *path_bak = '\0';
    TitleMetaData* tmd = (TitleMetaData*) malloc(TMD_SIZE_MAX);
    TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
    for (u32 i = 0; i < 8; i++) { // 8 is an arbitrary number
        char path_tmd[64];
        snprintf(path_tmd, sizeof(path_tmd), "%s/title/00040010/%08lx/content/%08lx.tmd", drv, tidlow_hs, i);
        if (LoadTmdFile(tmd, path_tmd) != 0) continue;
        if (!getbe16(tmd->content_count)) break;
        if (path_cxi) snprintf(path_cxi, 64, "%s/title/00040010/%08lx/content/%08lx.app", drv, tidlow_hs, getbe32(chunk->id));
        if (path_bak) snprintf(path_bak, 64, "%s/title/00040010/%08lx/content/%08lx.bak", drv, tidlow_hs, getbe32(chunk->id));
        break;
    }
    free(tmd);

    return ((path_cxi && !*path_cxi) || (path_bak && !*path_bak)) ? 1 : 0;
}

u32 CheckHealthAndSafetyInject(const char* hsdrv) {
    char path_bak[64] = { 0 };
    return ((GetHealthAndSafetyPaths(hsdrv, NULL, path_bak) == 0) &&
        (f_stat(path_bak, NULL) == FR_OK)) ? 0 : 1;
}

u32 InjectHealthAndSafety(const char* path, const char* destdrv) {
    NcchHeader ncch;
    NcchExtHeader exthdr;

    // write permissions
    if (!CheckWritePermissions(destdrv))
        return 1;

    // legacy stuff - remove mark file
    char path_mrk[32] = { 0 };
    snprintf(path_mrk, sizeof(path_mrk), "%s/%s", destdrv, "__gm9_hsbak.pth");
    f_unlink(path_mrk);

    // get H&S paths
    char path_cxi[64] = { 0 };
    char path_bak[64] = { 0 };
    if (GetHealthAndSafetyPaths(destdrv, path_cxi, path_bak) != 0) return 1;

    if (!path) { // if path == NULL -> restore H&S from backup
        if (f_stat(path_bak, NULL) != FR_OK) return 1;
        f_unlink(path_cxi);
        f_rename(path_bak, path_cxi);
        return 0;
    }

    // check input file / crypto
    if ((LoadNcchHeaders(&ncch, &exthdr, NULL, path, 0) != 0) ||
        !(NCCH_IS_CXI(&ncch)) || (SetupNcchCrypto(&ncch, NCCH_NOCRYPTO) != 0))
        return 1;

    // check crypto, get sig
    if ((LoadNcchHeaders(&ncch, &exthdr, NULL, path_cxi, 0) != 0) ||
        (SetupNcchCrypto(&ncch, NCCH_NOCRYPTO) != 0) || !(NCCH_IS_CXI(&ncch)))
        return 1;
    u8 sig[0x100];
    memcpy(sig, ncch.signature, 0x100);
    u16 crypto = NCCH_GET_CRYPTO(&ncch);
    u64 tid_hs = ncch.programId;

    // make a backup copy if there is not already one (point of no return)
    if (f_stat(path_bak, NULL) != FR_OK) {
        if (f_rename(path_cxi, path_bak) != FR_OK) return 1;
    } else f_unlink(path_cxi);

    // copy / decrypt the source CXI
    u32 ret = 0;
    if (CryptNcchNcsdBossFirmFile(path, path_cxi, GAME_NCCH, CRYPTO_DECRYPT, 0, 0, NULL, NULL) != 0)
        ret = 1;

    // fix up the injected H&S NCCH header / extheader (copy H&S signature, title ID to multiple locations)
    // also set savedata size to zero (thanks @TurdPooCharger)
    if ((ret == 0) && (LoadNcchHeaders(&ncch, &exthdr, NULL, path_cxi, 0) == 0)) {
        ncch.programId = tid_hs;
        ncch.partitionId = tid_hs;
        exthdr.jump_id = tid_hs;
        exthdr.aci_title_id = tid_hs;
        exthdr.aci_limit_title_id = tid_hs;
        exthdr.savedata_size = 0;
        memcpy(ncch.signature, sig, 0x100);
        sha_quick(ncch.hash_exthdr, &exthdr, 0x400, SHA256_MODE);
        if ((fvx_qwrite(path_cxi, &ncch, 0, sizeof(NcchHeader), NULL) != FR_OK) ||
            (fvx_qwrite(path_cxi, &exthdr, NCCH_EXTHDR_OFFSET, sizeof(NcchExtHeader), NULL) != FR_OK))
            ret = 1;
    } else ret = 1;

    // encrypt the CXI in place
    if (CryptNcchNcsdBossFirmFile(path_cxi, path_cxi, GAME_NCCH, crypto, 0, 0, NULL, NULL) != 0)
        ret = 1;

    if (ret != 0) { // in case of failure: try recover
        f_unlink(path_cxi);
        f_rename(path_bak, path_cxi);
    }

    return ret;
}

u32 BuildTitleKeyInfo(const char* path, bool dec, bool dump) {
    static TitleKeysInfo* tik_info = NULL;
    const char* path_out = (dec) ? OUTPUT_PATH "/" TIKDB_NAME_DEC : OUTPUT_PATH "/" TIKDB_NAME_ENC;
    const char* path_in = path;

    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;

    if (!path_in && !dump) { // no input path given - initialize
        if (!tik_info) tik_info = (TitleKeysInfo*) malloc(STD_BUFFER_SIZE);
        if (!tik_info) return 1;
        memset(tik_info, 0, 16);

        if ((fvx_stat(path_out, NULL) == FR_OK) &&
            (ShowPrompt(true, "%s\n%s", path_out, STR_OUTPUT_FILE_ALREADY_EXISTS_UPDATE_THIS)))
            path_in = path_out;
        else return 0;
    }

    u64 filetype = path_in ? IdentifyFileType(path_in) : 0;
    if (filetype & GAME_TICKET) {
        TicketCommon ticket;
        if ((fvx_qread(path_in, &ticket, 0, TICKET_COMMON_SIZE, NULL) != FR_OK) ||
            (TIKDB_SIZE(tik_info) + 32 > STD_BUFFER_SIZE) ||
            (AddTicketToInfo(tik_info, (Ticket*)&ticket, dec) != 0)) {
            return 1;
        }
    } else if (filetype & SYS_TICKDB) {
        u32 num_entries = 0;
        u8* title_ids = NULL;

        if (!InitImgFS(path_in) || 
            !(num_entries = GetNumTickets(PART_PATH)) ||
            !(title_ids = (u8*) malloc(num_entries * 8)) ||
            (ListTicketTitleIDs(PART_PATH, title_ids, num_entries) != 0)) {
            free(title_ids);
            InitImgFS(NULL);
            return 1;
        }

        // read and validate all tickets, add validated to info
        for (u32 i = 0; i < num_entries; i++) {
            Ticket* ticket;
            if (ReadTicketFromDB(PART_PATH, title_ids + (i * 8), &ticket) != 0) continue;
            if (ValidateTicketSignature(ticket) == 0)
                AddTicketToInfo(tik_info, ticket, dec); // ignore result
            free(ticket);
        }
        
        free(title_ids);
        InitImgFS(NULL);
    } else if (filetype & BIN_TIKDB) {
        TitleKeysInfo* tik_info_merge = (TitleKeysInfo*) malloc(STD_BUFFER_SIZE);
        if (!tik_info_merge) return 1;

        UINT br;
        if ((fvx_qread(path_in, tik_info_merge, 0, STD_BUFFER_SIZE, &br) != FR_OK) ||
            (TIKDB_SIZE(tik_info_merge) != br)) {
            free(tik_info_merge);
            return 1;
        }

        // merge and rebuild TitleKeyInfo
        u32 n_entries = tik_info_merge->n_entries;
        TitleKeyEntry* tik = tik_info_merge->entries;
        for (u32 i = 0; i < n_entries; i++, tik++) {
            if (TIKDB_SIZE(tik_info) + 32 > STD_BUFFER_SIZE) break; // no error message
            AddTitleKeyToInfo(tik_info, tik, !(filetype & FLAG_ENC), dec, false); // ignore result
        }

        free(tik_info_merge);
    }

    if (dump) {
        u32 dump_size = TIKDB_SIZE(tik_info);

        if (dump_size > 16) {
            if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) // ensure the output dir exists
                return 1;
            f_unlink(path_out);
            if ((dump_size <= 16) || (fvx_qwrite(path_out, tik_info, 0, dump_size, NULL) != FR_OK))
                return 1;
        }

        free(tik_info);
        tik_info = NULL;
    }

    return 0;
}

u32 BuildSeedInfo(const char* path, bool dump) {
    static SeedInfo* seed_info = NULL;
    const char* path_out = OUTPUT_PATH "/" SEEDINFO_NAME;
    const char* path_in = path;
    u32 inputtype = 0; // 0 -> none, 1 -> seeddb.bin, 2 -> seed system save

    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;

    if (!path_in && !dump) { // no input path given - initialize
        if (!seed_info) seed_info = (SeedInfo*) malloc(STD_BUFFER_SIZE);
        if (!seed_info) return 1;
        memset(seed_info, 0, 16);

        if ((fvx_stat(path_out, NULL) == FR_OK) &&
            (ShowPrompt(true, "%s\n%s", path_out, STR_OUTPUT_FILE_ALREADY_EXISTS_UPDATE_THIS))) {
            path_in = path_out;
            inputtype = 1;
        } else return 0;
    }

    // seed info has to be allocated at this point
    if (!seed_info) return 1;

    char path_str[128];
    if (path_in && (strnlen(path_in, 16) == 2)) { // when only a drive is given...
        if (GetSeedPath(path_str, path_in) != 0) return 1;
        path_in = path_str;
        inputtype = 2;
    }

    if (inputtype == 1) { // seeddb.bin input
        SeedInfo* seed_info_merge = (SeedInfo*) malloc(STD_BUFFER_SIZE);
        if (!seed_info_merge) return 1;

        UINT br;
        if ((fvx_qread(path_in, seed_info_merge, 0, STD_BUFFER_SIZE, &br) != FR_OK) ||
            (SEEDINFO_SIZE(seed_info_merge) != br)) {
            free(seed_info_merge);
            return 1;
        }

        // merge and rebuild SeedInfo
        u32 n_entries = seed_info_merge->n_entries;
        SeedInfoEntry* seed = seed_info_merge->entries;
        for (u32 i = 0; i < n_entries; i++, seed++) {
            if (SEEDINFO_SIZE(seed_info) + 32 > STD_BUFFER_SIZE) break; // no error message
            AddSeedToDb(seed_info, seed); // ignore result
        }

        free(seed_info_merge);
    } else if (inputtype == 2) { // seed system save input
        SeedDb* seedsave = (SeedDb*) malloc(sizeof(SeedDb));
        if (!seedsave) return 1;

        if ((ReadDisaDiffIvfcLvl4(path_in, NULL, SEEDSAVE_AREA_OFFSET, sizeof(SeedDb), seedsave) != sizeof(SeedDb)) ||
            (seedsave->n_entries >= SEEDSAVE_MAX_ENTRIES)) {
            free(seedsave);
            return 1;
        }

        SeedInfoEntry seed = { 0 };
        for (u32 s = 0; s < seedsave->n_entries; s++) {
            seed.titleId = seedsave->titleId[s];
            memcpy(&(seed.seed), &(seedsave->seed[s]), sizeof(Seed));
            if ((seed.titleId >> 32) != 0x00040000) continue;
            if (SEEDINFO_SIZE(seed_info) + 32 > STD_BUFFER_SIZE) break; // no error message
            AddSeedToDb(seed_info, &seed); // ignore result
        }

        free(seedsave);
    }

    if (dump) {
        u32 dump_size = SEEDINFO_SIZE(seed_info);
        u32 ret = 0;

        if (dump_size > 16) {
            if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) // ensure the output dir exists
                ret = 1;
            f_unlink(path_out);
            if (fvx_qwrite(path_out, seed_info, 0, dump_size, NULL) != FR_OK)
                ret = 1;
        } else ret = 1;

        free(seed_info);
        seed_info = NULL;
        return ret;
    }

    return 0;
}

u32 LoadNcchFromGameFile(const char* path, NcchHeader* ncch) {
    u64 filetype = IdentifyFileType(path);

    if (filetype & GAME_NCCH) {
        if ((fvx_qread(path, ncch, 0, sizeof(NcchHeader), NULL) == FR_OK) &&
            (ValidateNcchHeader(ncch) == 0)) return 0;
    } else if (filetype & GAME_NCSD) {
        if ((fvx_qread(path, ncch, NCSD_CNT0_OFFSET, sizeof(NcchHeader), NULL) == FR_OK) &&
            (ValidateNcchHeader(ncch) == 0)) return 0;
    } else if (filetype & GAME_CIA) {
        CiaStub* cia = (CiaStub*) malloc(sizeof(CiaStub));
        CiaInfo info;

        // load CIA stub from path
        if ((LoadCiaStub(cia, path) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0)) {
            free(cia);
            return 1;
        }

        // decrypt / load NCCH header from first CIA content
        u32 ret = 1;
        if (getbe16(cia->tmd.content_count)) {
            TmdContentChunk* chunk = cia->content_list;
            if ((getbe64(chunk->size) < sizeof(NcchHeader)) ||
                (fvx_qread(path, ncch, info.offset_content, sizeof(NcchHeader), NULL) != FR_OK)) {
                    free(cia);
                    return 1;
                }
            if (getbe16(chunk->type) & 0x1) { // decrypt first content header
                u8 titlekey[16];
                u8 ctr[16];
                GetTmdCtr(ctr, chunk);
                if (GetTitleKey(titlekey, (Ticket*)&(cia->ticket)) != 0) {
                    free(cia);
                    return 1;
                }
                DecryptCiaContentSequential((void*) ncch, sizeof(NcchHeader), ctr, titlekey);
            }
            if (ValidateNcchHeader(ncch) == 0) ret = 0;
        }

        free(cia);
        return ret;
    }

    return 1;
}

u32 GetGoodName(char* name, const char* path, bool quick) {
    // name should be 128+1 byte
    // name scheme (CTR+SMDH): <title_id> <title_name> (<product_code>) (<region>).<extension>
    // name scheme (CTR): <title_id> (<product_code>).<extension>
    // name scheme (NTR+ICON): <title_name> (<product_code>).<extension>
    // name scheme (TWL+ICON): <title_id> <title_name> (<product_code>) (<DSi unitcode>) (<region>).<extension>
    // name scheme (NTR): <name_short> (<product_code>).<extension>
    // name scheme (TWL): <title_id> (<product_code>).<extension>
    // name scheme (AGB): <name_short> (<product_code>).<extension>

    const char* path_donor = path;
    u64 type_donor = IdentifyFileType(path);
    char* ext =
        (type_donor & GAME_CIA)  ? "cia" :
        (type_donor & GAME_NCSD) ? "3ds" :
        (type_donor & GAME_NCCH) ? ((type_donor & FLAG_CXI) ? "cxi" : "cfa") :
        (type_donor & GAME_NDS)  ? "nds" :
        (type_donor & GAME_GBA)  ? "gba" :
        (type_donor & (GAME_TMD|GAME_CDNTMD|GAME_TWLTMD))  ? "tmd" :
        (type_donor & GAME_TIE)  ? "" : NULL;
    if (!ext) return 1;

    char appid_str[1 + 8 + 1] = { 0 }; // handling for NCCH / NDS in "?:/title" paths
    if ((type_donor & (GAME_NCCH|GAME_NDS)) && (strncmp(path + 1, ":/title/", 8) == 0)) {
        char* name = strrchr(path, '/');
        if (name && (strnlen(++name, 16) >= 8))
            *appid_str = '.';
        strncpy(appid_str + 1, name, 8);
    }

    char version_str[16] = { 0 };
    if (!quick && (type_donor & (GAME_CIA|GAME_TIE|GAME_TMD|GAME_CDNTMD|GAME_TWLTMD))) {
        u32 version = GetGameFileTitleVersion(path);
        if (version < 0x10000)
            snprintf(version_str, sizeof(version_str), " (v%lu.%lu.%lu)",
                (version>>10)&0x3F, (version>>4)&0x3F, version&0xF);
    }

    char path_content[256];
    if (type_donor & GAME_TMD) {
        if (GetTmdContentPath(path_content, path) != 0) return 1;
        path_donor = path_content;
        type_donor = IdentifyFileType(path_donor);
    } else if (type_donor & GAME_TIE) {
        if (GetTieContentPath(path_content, path) != 0) return 1;
        path_donor = path_content;
        type_donor = IdentifyFileType(path_donor);
    }

    void* data = malloc(0x1000 + max(sizeof(Smdh), sizeof(TwlIconData)));
    void* header = data;
    void* icon = ((u8*) data) + 0x1000;

    // load metadata
    if (type_donor & GAME_GBA) { // AGB
        if (fvx_qread(path_donor, header, 0, sizeof(AgbHeader), NULL) != FR_OK) type_donor = 0;
    } else if (type_donor & GAME_NDS) { // TWL
        if (LoadTwlMetaData(path_donor, (TwlHeader*) header,
            quick ? NULL : (TwlIconData*) icon) != 0) type_donor = 0;
    } else if (type_donor & (GAME_NCSD|GAME_NCCH)) { // CTR (data from NCCH)
        if (LoadNcchFromGameFile(path_donor, (NcchHeader*) header) != 0) type_donor = 0;
        if (!quick && (LoadSmdhFromGameFile(path_donor, (Smdh*) icon) != 0)) quick = true;
    } else if (type_donor & (GAME_CIA|GAME_CDNTMD|GAME_TWLTMD)) { // encrypted
        type_donor = LoadEncryptedIconFromCiaTmd(path_donor, icon, header, false);
    }

    // generate name
    if (type_donor & GAME_GBA) { // AGB
        AgbHeader* agb = (AgbHeader*) header;
        snprintf(name, 128, "%.12s (AGB-%.4s).%s", agb->game_title, agb->game_code, ext);
    } else if (type_donor & GAME_NDS) { // NTR or TWL
        TwlHeader* twl = (TwlHeader*) header;
        if (quick) {
            if (twl->unit_code & 0x02) { // TWL
                snprintf(name, 128, "%016llX%s (TWL-%.4s).%s", twl->title_id, appid_str, twl->game_code, ext);
            } else { // NTR
                snprintf(name, 128, "%.12s (NTR-%.4s).%s", twl->game_title, twl->game_code, ext);
            }
        } else {
            char title_name[0x80+1] = { 0 };
            if (GetTwlTitle(title_name, (TwlIconData*) icon) != 0) return 1;
            // search for the last occurence of newline, because
            // everything until the last newline is part of the name
            char* linebrk = strrchr(title_name, '\n');
            if (linebrk) {
                *linebrk = '\0';
                // replace any remaining newlines with a space, to merge the parts
                for (char* c = title_name; *c; c++)
                    if (*c == '\n') *c = ' ';
            }

            if (twl->unit_code & 0x02) { // TWL
                char region[8] = { 0 };
                if (twl->region_flags == TWL_REGION_FREE) snprintf(region, sizeof(region), "W");
                snprintf(region, sizeof(region), "%s%s%s%s%s",
                    (twl->region_flags & REGION_MASK_JPN) ? "J" : "",
                    (twl->region_flags & REGION_MASK_USA) ? "U" : "",
                    (twl->region_flags & REGION_MASK_EUR) ? "E" : "",
                    (twl->region_flags & REGION_MASK_CHN) ? "C" : "",
                    (twl->region_flags & REGION_MASK_KOR) ? "K" : "");
                if (strncmp(region, "JUECK", 8) == 0) snprintf(region, sizeof(region), "W");
                if (!*region) snprintf(region, sizeof(region), "UNK");

                const char* unit_str = (twl->unit_code == TWL_UNITCODE_TWLNTR) ? STR_DSI_ENHANCED : STR_DSI_EXCLUSIVE;
                snprintf(name, 128, "%016llX%s %s (TWL-%.4s) (%s) (%s)%s.%s",
                    twl->title_id, appid_str, title_name, twl->game_code, unit_str, region, version_str, ext);
            } else { // NTR
                snprintf(name, 128, "%s (NTR-%.4s).%s", title_name, twl->game_code, ext);
            }
        }
    } else if (type_donor & (GAME_NCSD|GAME_NCCH)) { // CTR (data from NCCH)
        NcchHeader* ncch = (NcchHeader*) header;
        if (quick) snprintf(name, 128, "%016llX%s (%.16s).%s", ncch->programId, appid_str, ncch->productcode, ext);
        else {
            Smdh* smdh = (Smdh*) icon;
            char title_name[0x40+1] = { 0 };
            if (GetSmdhDescShort(title_name, smdh) != 0) return 1;

            char region[8] = { 0 };
            if (smdh->region_lockout == SMDH_REGION_FREE) snprintf(region, sizeof(region), "W");
            else snprintf(region, sizeof(region), "%s%s%s%s%s%s",
                (smdh->region_lockout & REGION_MASK_JPN) ? "J" : "",
                (smdh->region_lockout & REGION_MASK_USA) ? "U" : "",
                (smdh->region_lockout & REGION_MASK_EUR) ? "E" : "",
                (smdh->region_lockout & REGION_MASK_CHN) ? "C" : "",
                (smdh->region_lockout & REGION_MASK_KOR) ? "K" : "",
                (smdh->region_lockout & REGION_MASK_TWN) ? "T" : "");
            if (strncmp(region, "JUECKT", 8) == 0) snprintf(region, sizeof(region), "W");
            if (!*region) snprintf(region, sizeof(region), "UNK");

            snprintf(name, 128, "%016llX%s %s (%.16s) (%s)%s.%s",
                ncch->programId, appid_str, title_name, ncch->productcode, region, version_str, ext);
        }
    }

    free(data);
    if (!type_donor) return 1;

    // remove illegal chars from filename
    for (char* c = name; *c; c++) {
        if ((*c == ':') || (*c == '/') || (*c == '\\') || (*c == '"') ||
            (*c == '*') || (*c == '?') || (*c == '\n') || (*c == '\r'))
            *c = ' ';
        if ((*c == '.') && !*(c+1))
            *c = '\0';
    }

    // remove double spaces from filename
    char* s = name;
    for (char* c = name; *s; c++, s++) {
        while ((*c == ' ') && (*(c+1) == ' ')) c++;
        *s = *c;
    }

    return 0;
}
