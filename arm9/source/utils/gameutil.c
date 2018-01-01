#include "gameutil.h"
#include "game.h"
#include "hid.h"
#include "ui.h"
#include "fs.h"
#include "unittype.h"
#include "aes.h"
#include "sha.h"

// use NCCH crypto defines for everything 
#define CRYPTO_DECRYPT  NCCH_NOCRYPTO
#define CRYPTO_ENCRYPT  NCCH_STDCRYPTO

u32 GetNcchHeaders(NcchHeader* ncch, NcchExtHeader* exthdr, ExeFsHeader* exefs, FIL* file) {
    u32 offset_ncch = fvx_tell(file);
    UINT btr;
    
    if ((fvx_read(file, ncch, sizeof(NcchHeader), &btr) != FR_OK) ||
        (ValidateNcchHeader(ncch) != 0))
        return 1;
    
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
    
    sha_init(SHA256_MODE);
    for (u32 i = 0; i < size_data; i += MAIN_BUFFER_SIZE) {
        u32 read_bytes = min(MAIN_BUFFER_SIZE, (size_data - i));
        UINT bytes_read;
        fvx_read(file, MAIN_BUFFER, read_bytes, &bytes_read);
        DecryptNcch(MAIN_BUFFER, offset_data + i, read_bytes, ncch, exefs);
        sha_update(MAIN_BUFFER, read_bytes);
    }
    sha_get(hash);
    
    return (memcmp(hash, expected, 32) == 0) ? 0 : 1;
}

u32 LoadNcchHeaders(NcchHeader* ncch, NcchExtHeader* exthdr, ExeFsHeader* exefs, const char* path, u32 offset) {
    FIL file;
    
    // open file, get NCCH header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, offset);
    if (GetNcchHeaders(ncch, exthdr, exefs, &file) != 0) {
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
    if ((GetNcchHeaders(&ncch, NULL, &exefs, &file) != 0) ||
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
    const u8 magic[] = { TMD_SIG_TYPE };
    UINT br;
    
    // full TMD file
    if ((fvx_qread(path, tmd, 0, TMD_SIZE_MAX, &br) != FR_OK) ||
        (memcmp(tmd->sig_type, magic, sizeof(magic)) != 0) ||
        (br < TMD_SIZE_N(getbe16(tmd->content_count))))
        return 1;
    
    return 0;
}

u32 LoadCdnTicketFile(Ticket* ticket, const char* path_cnt) {
    // path points to CDN content file
    char path_cetk[256];
    strncpy(path_cetk, path_cnt, 256);
    char* name_cetk = strrchr(path_cetk, '/');
    if (!name_cetk) return 1; // will not happen
    char* ext_cetk = strrchr(++name_cetk, '.');
    ext_cetk = (ext_cetk) ? ext_cetk + 1 : name_cetk;
    snprintf(ext_cetk, 256 - (ext_cetk - path_cetk), "cetk");
    
    // load and check ticket
    UINT br;
    if ((fvx_qread(path_cetk, ticket, 0, TICKET_SIZE, &br) != FR_OK) || (br != TICKET_SIZE) ||
        (ValidateTicket(ticket) != 0)) return 1;
        
    return 0;
}

u32 GetTmdContentPath(char* path_content, const char* path_tmd) {
    // get path to TMD first content
    const u8 dlc_tid_high[] = { DLC_TID_HIGH };
    TitleMetaData* tmd = (TitleMetaData*) TEMP_BUFFER;
    TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
    
    // content path string
    char* name_content;
    strncpy(path_content, path_tmd, 256);
    name_content = strrchr(path_content, '/');
    if (!name_content) return 1; // will not happen
    name_content++;
    
    // load TMD file
    if ((LoadTmdFile(tmd, path_tmd) != 0) || !getbe16(tmd->content_count))
        return 1;
    snprintf(name_content, 256 - (name_content - path_content),
        (memcmp(tmd->title_id, dlc_tid_high, sizeof(dlc_tid_high)) == 0) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
    
    return 0;
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
    u8 hash[32];
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
    
    GetTmdCtr(ctr, chunk);
    sha_init(SHA256_MODE);
    for (u32 i = 0; i < size; i += MAIN_BUFFER_SIZE) {
        u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
        UINT bytes_read;
        fvx_read(&file, MAIN_BUFFER, read_bytes, &bytes_read);
        if (encrypted) DecryptCiaContentSequential(MAIN_BUFFER, read_bytes, ctr, titlekey);
        sha_update(MAIN_BUFFER, read_bytes);
        if (!ShowProgress(i + read_bytes, size, path)) break;
    }
    sha_get(hash);
    fvx_close(&file);
    
    return memcmp(hash, expected, 32);
}

u32 VerifyNcchFile(const char* path, u32 offset, u32 size) {
    NcchHeader ncch;
    ExeFsHeader exefs;
    FIL file;
    
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // open file, get NCCH, ExeFS header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    fvx_lseek(&file, offset);
    if (GetNcchHeaders(&ncch, NULL, NULL, &file) != 0) {
        if (!offset) ShowPrompt(false, "%s\nError: Not a NCCH file", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    fvx_lseek(&file, offset);
    if (ncch.size_exefs && (GetNcchHeaders(&ncch, NULL, &exefs, &file) != 0)) {
        if (!offset) ShowPrompt(false, "%s\nError: Bad ExeFS header", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // size checks
    if (!size) size = fvx_size(&file) - offset;
    if ((fvx_size(&file) < offset) || (size < ncch.size * NCCH_MEDIA_UNIT)) {
        if (!offset) ShowPrompt(false, "%s\nError: File is too small", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // check / setup crypto
    if (SetupNcchCrypto(&ncch, NCCH_NOCRYPTO) != 0) {
        if (!offset) ShowPrompt(false, "%s\nError: Crypto not set up", pathstr);
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
    
    // thorough exefs verification
    if (ncch.size_exefs > 0) {
        for (u32 i = 0; !ver_exefs && (i < 10); i++) {
            ExeFsFileHeader* exefile = exefs.files + i;
            u8* hash = exefs.hashes[9 - i];
            if (!exefile->size) continue;
            fvx_lseek(&file, offset + (ncch.offset_exefs * NCCH_MEDIA_UNIT) + 0x200 + exefile->offset);
            ver_exefs = CheckNcchHash(hash, &file, exefile->size, offset, &ncch, &exefs);
        }
    }
    
    if (!offset && (ver_exthdr|ver_exefs|ver_romfs)) { // verification summary
        ShowPrompt(false, "%s\nNCCH verification failed:\nExtHdr/ExeFS/RomFS: %s/%s/%s", pathstr,
            (!ncch.size_exthdr) ? "-" : (ver_exthdr == 0) ? "ok" : "fail",
            (!ncch.size_exefs) ? "-" : (ver_exefs == 0) ? "ok" : "fail",
            (!ncch.size_romfs) ? "-" : (ver_romfs == 0) ? "ok" : "fail");
    }
    
    fvx_close(&file);
    return ver_exthdr|ver_exefs|ver_romfs;
}

u32 VerifyNcsdFile(const char* path) {
    NcsdHeader ncsd;
    
    // path string
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // load NCSD header
    if (LoadNcsdHeader(&ncsd, path) != 0) {
        ShowPrompt(false, "%s\nError: Not a NCSD file", pathstr);
        return 1;
    }
    
    // validate NCSD contents
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd.partitions + i;
        u32 offset = partition->offset * NCSD_MEDIA_UNIT;
        u32 size = partition->size * NCSD_MEDIA_UNIT;
        if (!size) continue;
        if (VerifyNcchFile(path, offset, size) != 0) {
            ShowPrompt(false, "%s\nContent%lu (%08lX@%08lX):\nVerification failed",
                pathstr, i, size, offset, i);
            return 1;
        }
    }
    
    return 0;
}

u32 VerifyCiaFile(const char* path) {
    CiaStub* cia = (CiaStub*) TEMP_BUFFER;
    CiaInfo info;
    u8 titlekey[16];
    
     // path string
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // load CIA stub
    if ((LoadCiaStub(cia, path) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0) ||
        (GetTitleKey(titlekey, &(cia->ticket)) != 0)) {
        ShowPrompt(false, "%s\nError: Probably not a CIA file", pathstr);
        return 1;
    }
    
    // verify contents
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        if (VerifyTmdContent(path, next_offset, chunk, titlekey) != 0) {
            ShowPrompt(false, "%s\nID %08lX (%08llX@%08llX)\nVerification failed",
                pathstr, getbe32(chunk->id), getbe64(chunk->size), next_offset, i);
            return 1;
        }
        next_offset += getbe64(chunk->size);
    }
    
    return 0;
}

u32 VerifyTmdFile(const char* path, bool cdn) {
    const u8 dlc_tid_high[] = { DLC_TID_HIGH };
    TitleMetaData* tmd = (TitleMetaData*) TEMP_BUFFER;
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    Ticket* ticket = (Ticket*) (TEMP_BUFFER + TMD_SIZE_MAX);
    u8 titlekey[0x10] = { 0xFF };
    
    // path string
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // content path string
    char path_content[256];
    char* name_content;
    strncpy(path_content, path, 256);
    name_content = strrchr(path_content, '/');
    if (!name_content) return 1; // will not happen
    name_content++;
    
    // load TMD file
    if (LoadTmdFile(tmd, path) != 0) {
        ShowPrompt(false, "%s\nError: TMD probably corrupted", pathstr);
        return 1;
    }
    
    if (cdn) { // load / build ticket (for titlekey / CDN only)
        if (!((LoadCdnTicketFile(ticket, path) == 0) ||
             ((BuildFakeTicket(ticket, tmd->title_id) == 0) &&
             (FindTitleKey(ticket, tmd->title_id) == 0))) ||
            (GetTitleKey(titlekey, ticket) != 0)) {
            ShowPrompt(false, "%s\nError: CDN titlekey not found", pathstr);
            return 1;
        }
    }
    
    // verify contents
    u32 content_count = getbe16(tmd->content_count);
    bool dlc = !cdn && (memcmp(tmd->title_id, dlc_tid_high, sizeof(dlc_tid_high)) == 0);
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(content_list[i]);
        if (!cdn) chunk->type[1] &= ~0x01; // remove crypto flag
        snprintf(name_content, 256 - (name_content - path_content),
            (cdn) ? "%08lx" : (dlc) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
        TruncateString(pathstr, path_content, 32, 8);
        if (VerifyTmdContent(path_content, 0, chunk, titlekey) != 0) {
            ShowPrompt(false, "%s\nVerification failed", pathstr);
            return 1;
        }
    }
    
    return 0;
}

u32 VerifyFirmFile(const char* path) {
    FirmHeader header;
    FIL file;
    UINT btr;
    
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // open file, get FIRM header
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, &header, sizeof(FirmHeader), &btr) != FR_OK) ||
        (ValidateFirmHeader(&header, fvx_size(&file)) != 0)) {
        fvx_close(&file);
        return 1;
    }
    
    // hash verify all available sections
    for (u32 i = 0; i < 4; i++) {
        FirmSectionHeader* section = header.sections + i;
        u32 size = section->size;
        if (!size) continue;
        fvx_lseek(&file, section->offset);
        sha_init(SHA256_MODE);
        for (u32 i = 0; i < size; i += MAIN_BUFFER_SIZE) {
            u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
            fvx_read(&file, MAIN_BUFFER, read_bytes, &btr);
            sha_update(MAIN_BUFFER, read_bytes);
        }
        u8 hash[0x20];
        sha_get(hash);
        if (memcmp(hash, section->hash, 0x20) != 0) {
            ShowPrompt(false, "%s\nSection %u hash mismatch", pathstr, i);
            fvx_close(&file);
            return 1;
        }
    }
    fvx_close(&file);
    
    // no arm11 / arm9 entrypoints?
    if (!header.entry_arm9) {
        ShowPrompt(false, "%s\nARM9 entrypoint is missing", pathstr);
        return 1;
    } else if (!header.entry_arm11) {
        ShowPrompt(false, "%s\nWarning: ARM11 entrypoint is missing", pathstr);
    }
    
    return 0;
}

u32 VerifyBossFile(const char* path) {
    BossHeader* boss = (BossHeader*) TEMP_BUFFER;
    u32 payload_size;
    bool encrypted = false;
    FIL file;
    UINT btr;
    
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // read file header
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, boss, sizeof(BossHeader), &btr) != FR_OK) ||
        (btr != sizeof(BossHeader)) || (ValidateBossHeader(boss, 0) != 0)) {
        ShowPrompt(false, "%s\nError: Not a BOSS file", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // get / check size
    payload_size = getbe32(boss->filesize) - sizeof(BossHeader);
    if (!payload_size) {
        fvx_close(&file);
        return 1;
    }
    
    // check if encrypted, decrypt if required
    encrypted = (CheckBossEncrypted(boss) == 0);
    if (encrypted) CryptBoss((u8*) boss, 0, sizeof(BossHeader), boss);
    
    // actual hash calculation & compare
    u8 hash[32];
    sha_init(SHA256_MODE);
    GetBossPayloadHashHeader(MAIN_BUFFER, boss);
    u32 read_bytes = min((MAIN_BUFFER_SIZE - BOSS_SIZE_PAYLOAD_HEADER), payload_size);
    fvx_read(&file, MAIN_BUFFER + BOSS_SIZE_PAYLOAD_HEADER, read_bytes, &btr);
    if (encrypted) CryptBoss(MAIN_BUFFER + BOSS_SIZE_PAYLOAD_HEADER, sizeof(BossHeader), read_bytes, boss);
    sha_update(MAIN_BUFFER, read_bytes + BOSS_SIZE_PAYLOAD_HEADER);
    for (u32 i = read_bytes; i < payload_size; i += MAIN_BUFFER_SIZE) {
        read_bytes = min(MAIN_BUFFER_SIZE, (payload_size - i));
        fvx_read(&file, MAIN_BUFFER, read_bytes, &btr);
        if (encrypted) CryptBoss(MAIN_BUFFER, sizeof(BossHeader) + i, read_bytes, boss);
        sha_update(MAIN_BUFFER, read_bytes);
    }
    fvx_close(&file);
    sha_get(hash);
    if (memcmp(hash, boss->hash_payload, 0x20) != 0) {
        ShowPrompt(false, "%s\nBOSS payload hash mismatch", pathstr);
        return 1;
    }
    
    return 0;
}

u32 VerifyGameFile(const char* path) {
    u64 filetype = IdentifyFileType(path);
    if (filetype & GAME_CIA)
        return VerifyCiaFile(path);
    else if (filetype & GAME_NCSD)
        return VerifyNcsdFile(path);
    else if (filetype & GAME_NCCH)
        return VerifyNcchFile(path, 0, 0);
    else if (filetype & GAME_TMD)
        return VerifyTmdFile(path, filetype & FLAG_NUSCDN);
    else if (filetype & GAME_BOSS)
        return VerifyBossFile(path);
    else if (filetype & SYS_FIRM)
        return VerifyFirmFile(path);
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
    CiaStub* cia = (CiaStub*) TEMP_BUFFER;
    CiaInfo info;
    
    // load CIA stub
    if ((LoadCiaStub(cia, path) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0))
        return 1;
    
    // check for encryption in CIA contents
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        if ((getbe16(chunk->type) & 0x1) || (CheckEncryptedNcchFile(path, next_offset) == 0))
            return 0; // encryption found
        next_offset += getbe64(chunk->size);
    }
    
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
    BossHeader* boss = (BossHeader*) TEMP_BUFFER;
    UINT btr;
    
    // get boss header
    if ((fvx_qread(path, boss, 0, sizeof(BossHeader), &btr) != FR_OK) ||
        (btr != sizeof(BossHeader))) {
        return 1;
    }
    
    return CheckBossEncrypted(boss);
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
        return 0; // these should always be encrypted
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
    
    u32 ret = 0;
    if (!ShowProgress(offset, fsize, dest)) ret = 1;
    if (mode & (GAME_NCCH|GAME_NCSD|GAME_BOSS|SYS_FIRM|GAME_NDS)) { // for NCCH / NCSD / BOSS / FIRM files
        for (u64 i = 0; (i < size) && (ret == 0); i += MAIN_BUFFER_SIZE) {
            u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
            UINT bytes_read, bytes_written;
            if (fvx_read(ofp, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK) ret = 1;
            if (((mode & GAME_NCCH) && (CryptNcchSequential(MAIN_BUFFER, i, read_bytes, crypto) != 0)) ||
                ((mode & GAME_NCSD) && (CryptNcsdSequential(MAIN_BUFFER, i, read_bytes, crypto) != 0)) ||
                ((mode & GAME_BOSS) && crypt_boss && (CryptBossSequential(MAIN_BUFFER, i, read_bytes) != 0)) ||
                ((mode & SYS_FIRM) && (DecryptFirmSequential(MAIN_BUFFER, i, read_bytes) != 0)))
                ret = 1;
            if (inplace) fvx_lseek(ofp, fvx_tell(ofp) - read_bytes);
            if (fvx_write(dfp, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK) ret = 1;
            if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
            if (!ShowProgress(offset + i + read_bytes, fsize, dest)) ret = 1;
        }
    } else if (mode & (GAME_CIA|GAME_NUSCDN)) { // for NCCHs inside CIAs
        bool cia_crypto = getbe16(chunk->type) & 0x1;
        bool ncch_crypto; // find out by decrypting the NCCH header
        UINT bytes_read, bytes_written;
        u8 ctr[16];
        
        NcchHeader* ncch = (NcchHeader*) (void*) MAIN_BUFFER;
        GetTmdCtr(ctr, chunk); // NCCH crypto?
        if (fvx_read(ofp, MAIN_BUFFER, sizeof(NcchHeader), &bytes_read) != FR_OK) ret = 1;
        if (cia_crypto) DecryptCiaContentSequential(MAIN_BUFFER, sizeof(NcchHeader), ctr, titlekey);
        ncch_crypto = ((ValidateNcchHeader(ncch) == 0) && (NCCH_ENCRYPTED(ncch) || !(crypto & NCCH_NOCRYPTO)));
        if (ncch_crypto && (SetupNcchCrypto(ncch, crypto) != 0))
            ret = 1;
        
        GetTmdCtr(ctr, chunk);
        fvx_lseek(ofp, offset);
        sha_init(SHA256_MODE);
        for (u64 i = 0; (i < size) && (ret == 0); i += MAIN_BUFFER_SIZE) {
            u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
            if (fvx_read(ofp, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK) ret = 1;
            if (cia_crypto && (DecryptCiaContentSequential(MAIN_BUFFER, read_bytes, ctr, titlekey) != 0)) ret = 1;
            if (ncch_crypto && (CryptNcchSequential(MAIN_BUFFER, i, read_bytes, crypto) != 0)) ret = 1;
            if (inplace) fvx_lseek(ofp, fvx_tell(ofp) - read_bytes);
            if (fvx_write(dfp, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK) ret = 1;
            sha_update(MAIN_BUFFER, read_bytes);
            if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
            if (!ShowProgress(offset + i + read_bytes, fsize, dest)) ret = 1;
        }
        sha_get(chunk->hash);
        chunk->type[1] &= ~0x01;
    }
    
    fvx_close(ofp);
    if (!inplace) fvx_close(dfp);
    
    return ret;
}

u32 CryptCiaFile(const char* orig, const char* dest, u16 crypto) {
    bool inplace = (strncmp(orig, dest, 256) == 0);
    CiaStub* cia = (CiaStub*) TEMP_BUFFER;
    CiaInfo info;
    u8 titlekey[16];
    
    // start operation
    if (!ShowProgress(0, 0, orig)) return 1;
    
    // if not inplace: clear destination
    if (!inplace) f_unlink(dest);
    
    // load CIA stub from origin
    if ((LoadCiaStub(cia, orig) != 0) ||
        (GetCiaInfo(&info, &(cia->header)) != 0) ||
        (GetTitleKey(titlekey, &(cia->ticket)) != 0)) {
        return 1;
    }
    
    // decrypt CIA contents
    u32 content_count = getbe16(cia->tmd.content_count);
    u64 next_offset = info.offset_content;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        u64 size = getbe64(chunk->size);
        if (CryptNcchNcsdBossFirmFile(orig, dest, GAME_CIA, crypto, next_offset, size, chunk, titlekey) != 0)
            return 1;
        next_offset += size;
    }
    
    // if not inplace: take over CIA metadata
    if (!inplace && (info.size_meta == CIA_META_SIZE)) {
        CiaMeta* meta = (CiaMeta*) (void*) (cia + 1);
        if ((fvx_qread(orig, meta, info.offset_meta, CIA_META_SIZE, NULL) != FR_OK) ||
            (fvx_qwrite(dest, meta, info.offset_meta, CIA_META_SIZE, NULL) != FR_OK))
            return 1;
    }
    
    // fix TMD hashes, write CIA stub to destination
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, dest) != 0)) return 1;
    
    return 0;
}

u32 DecryptFirmFile(const char* orig, const char* dest) {
    const u8 dec_magic[] = { 'D', 'E', 'C', '\0' }; // insert to decrypted firms
    FirmHeader firm;
    FIL file;
    UINT btr;
    
    // actual decryption
    if (CryptNcchNcsdBossFirmFile(orig, dest, SYS_FIRM, CRYPTO_DECRYPT, 0, 0, NULL, NULL) != 0)
        return 1;
    
    // open destination file, get FIRM header
    if (fvx_open(&file, dest, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    fvx_lseek(&file, 0);
    if ((fvx_read(&file, &firm, sizeof(FirmHeader), &btr) != FR_OK) ||
        (ValidateFirmHeader(&firm, fvx_size(&file)) != 0)) {
        fvx_close(&file);
        return 1;
    }
    
    // find ARM9 section
    FirmSectionHeader* arm9s = FindFirmArm9Section(&firm);
    if (!arm9s || !arm9s->size) return 1;
    
    // decrypt ARM9 loader header
    FirmA9LHeader a9l;
    fvx_lseek(&file, arm9s->offset);
    if ((fvx_read(&file, &a9l, sizeof(FirmA9LHeader), &btr) != FR_OK) ||
        (DecryptA9LHeader(&a9l) != 0) || (fvx_lseek(&file, arm9s->offset) != FR_OK) ||
        (fvx_write(&file, &a9l, sizeof(FirmA9LHeader), &btr) != FR_OK)) {
        fvx_close(&file);
        return 1;
    }
    
    // calculate new hash for ARM9 section 
    fvx_lseek(&file, arm9s->offset);
    sha_init(SHA256_MODE);
    for (u32 i = 0; i < arm9s->size; i += MAIN_BUFFER_SIZE) {
        u32 read_bytes = min(MAIN_BUFFER_SIZE, (arm9s->size - i));
        if ((fvx_read(&file, MAIN_BUFFER, read_bytes, &btr) != FR_OK) || (btr != read_bytes)) {
            fvx_close(&file);
            return 1;
        }
        sha_update(MAIN_BUFFER, read_bytes);
    }
    sha_get(arm9s->hash);
    
    // write back FIRM header
    fvx_lseek(&file, 0);
    memcpy(firm.dec_magic, dec_magic, sizeof(dec_magic));
    firm.entry_arm9 = ARM9ENTRY_FIX(&firm);
    if (fvx_write(&file, &firm, sizeof(FirmHeader), &btr) != FR_OK) {
        fvx_close(&file);
        return 1;
    }
    
    fvx_close(&file);
    return 0;
}

u32 CryptCdnFile(const char* orig, const char* dest, u16 crypto) {
    bool inplace = (strncmp(orig, dest, 256) == 0);
    TitleMetaData* tmd = (TitleMetaData*) TEMP_BUFFER;
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    Ticket* ticket = (Ticket*) (TEMP_BUFFER + TMD_SIZE_MAX);
    u8 titlekey[0x10] = { 0xFF };
    
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
        name_tmd = strrchr(path_tmd, '/');
        if (!name_tmd) return 1; // will not happen
        name_tmd++;
        snprintf(name_tmd, 256 - (name_tmd - path_tmd), "tmd");
        if (LoadTmdFile(tmd, path_tmd) != 0) tmd = NULL;
    } else tmd = NULL;
    
    // load or build ticket
    if (LoadCdnTicketFile(ticket, orig) != 0) {
        if (!tmd || (BuildFakeTicket(ticket, tmd->title_id) != 0)) return 1;
        if (FindTitleKey(ticket, tmd->title_id) != 0) return 1;
    }
    
    // get titlekey
    if (GetTitleKey(titlekey, ticket) != 0)
        return 1;
    
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
    if (CryptNcchNcsdBossFirmFile(orig, dest, GAME_NUSCDN, crypto, 0, 0, chunk, titlekey) != 0)
        return 1;
    
    if (inplace && tmd) {
        UINT bw; // in that case, write the change to the TMD file, too
        u32 offset = ((u8*) chunk) - ((u8*) tmd);
        fvx_qwrite(path_tmd, chunk, offset, sizeof(TmdContentChunk), &bw);
    }
    
    return 0;
}

u32 CryptGameFile(const char* path, bool inplace, bool encrypt) {
    u64 filetype = IdentifyFileType(path);
    u16 crypto = encrypt ? CRYPTO_ENCRYPT : CRYPTO_DECRYPT;
    char dest[256];
    char* destptr = (char*) path;
    u32 ret = 0;
    
    if (!inplace) { // build output name
        // build output name
        snprintf(dest, 256, OUTPUT_PATH "/");
        char* dname = dest + strnlen(dest, 256);
        if ((strncmp(path + 1, ":/title/", 8) != 0) || (GetGoodName(dname, path, false) != 0)) {
            char* name = strrchr(path, '/');
            if (!name) return 1;
            snprintf(dest, 256, "%s/%s", OUTPUT_PATH, ++name);
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
    
    // main loop starts here
    u8 ctr_in[16];
    u8 ctr_out[16];
    u32 ret = 0;
    GetTmdCtr(ctr_in, chunk);
    GetTmdCtr(ctr_out, chunk);
    if (!ShowProgress(0, 0, path_content)) ret = 1;
    for (u32 i = 0; (i < size) && (ret == 0); i += MAIN_BUFFER_SIZE) {
        u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
        if (fvx_read(&ofile, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK) ret = 1;
        if (cdn_decrypt && (DecryptCiaContentSequential(MAIN_BUFFER, read_bytes, ctr_in, titlekey) != 0)) ret = 1;
        if (ncch_decrypt && (DecryptNcchSequential(MAIN_BUFFER, i, read_bytes) != 0)) ret = 1;
        if ((i == 0) && cxi_fix && (SetNcchSdFlag(MAIN_BUFFER) != 0)) ret = 1;
        if (i == 0) sha_init(SHA256_MODE);
        sha_update(MAIN_BUFFER, read_bytes);
        if (cia_encrypt && (EncryptCiaContentSequential(MAIN_BUFFER, read_bytes, ctr_out, titlekey) != 0)) ret = 1;
        if (fvx_write(&dfile, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK) ret = 1;
        if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
        if (!ShowProgress(offset + i + read_bytes, fsize, path_content)) ret = 1;
    }
    u8 hash[0x20];
    sha_get(hash);
    
    fvx_close(&ofile);
    fvx_close(&dfile);
    
    // force legit?
    if (force_legit && (memcmp(hash, chunk->hash, 0x20) != 0)) return 1;
    if (force_legit && (getbe64(chunk->size) != size)) return 1;
    
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

u32 BuildCiaFromTmdFile(const char* path_tmd, const char* path_cia, bool force_legit, bool cdn) {
    const u8 dlc_tid_high[] = { DLC_TID_HIGH };
    CiaStub* cia = (CiaStub*) TEMP_BUFFER;
    CiaMeta* meta = (CiaMeta*) (TEMP_BUFFER + sizeof(CiaStub));
    
    // Init progress bar
    if (!ShowProgress(0, 0, path_tmd)) return 1;
    
    // build the CIA stub
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header)) != 0) ||
        (LoadTmdFile(&(cia->tmd), path_tmd) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildFakeTicket(&(cia->ticket), cia->tmd.title_id) != 0)) {
        return 1;
    }
    
    // extract info from TMD
    TitleMetaData* tmd = &(cia->tmd);
    TmdContentChunk* content_list = cia->content_list;
    u32 content_count = getbe16(tmd->content_count);
    u8* title_id = tmd->title_id;
    bool dlc = (memcmp(tmd->title_id, dlc_tid_high, sizeof(dlc_tid_high)) == 0);
    if (!content_count) return 1;
    
    // get (legit) ticket
    Ticket* ticket = &(cia->ticket);
    bool src_emunand = ((*path_tmd == 'B') || (*path_tmd == '4'));
    if (force_legit) {
        if ((cdn && (LoadCdnTicketFile(ticket, path_tmd) != 0)) ||
            (!cdn && (FindTicket(ticket, title_id, true, src_emunand) != 0))) {
            ShowPrompt(false, "ID %016llX\nLegit ticket not found.", getbe64(title_id));
            return 1;
        }
    } else if (cdn) {
        if ((LoadCdnTicketFile(ticket, path_tmd) != 0) &&
            (FindTitleKey(ticket, title_id) != 0)) {
            ShowPrompt(false, "ID %016llX\nTitlekey not found.", getbe64(title_id));
            return 1;
        }
    } else {
        if ((FindTitleKey(ticket, title_id) != 0) && 
            (FindTicket(ticket, title_id, false, src_emunand) == 0) &&
            (getbe32(ticket->console_id) || getbe32(ticket->eshop_id))) {
            // if ticket found: wipe private data
            memset(ticket->console_id, 0, 4); // zero out console id
            memset(ticket->eshop_id, 0, 4); // zero out eshop id
            memset(ticket->ticket_id, 0, 8); // zero out ticket id
        }
    }
    
    // content path string
    char path_content[256];
    char* name_content;
    strncpy(path_content, path_tmd, 256);
    name_content = strrchr(path_content, '/');
    if (!name_content) return 1; // will not happen
    name_content++;
    
    // DLC? Check for missing contents first!
    if (dlc) for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS);) {
        FILINFO fno;
        TmdContentChunk* chunk = &(content_list[i]);
        snprintf(name_content, 256 - (name_content - path_content),
            (cdn) ? "%08lx" : (dlc && !cdn) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
        if ((fvx_stat(path_content, &fno) != FR_OK) || (fno.fsize != (u32) getbe64(chunk->size)))
            memmove(chunk, chunk + 1, ((--content_count) - i) * sizeof(TmdContentChunk));
        else i++;
    }
    if (!content_count) return 1;
    if (content_count < (u16) getbe16(tmd->content_count)) {
        if (!ShowPrompt(true, "ID %016llX\nIncomplete DLC (%u missing)\nContinue?",
            getbe64(title_id), getbe16(tmd->content_count) - content_count)) return 1;
        tmd->content_count[0] = (content_count >> 8) & 0xFF;
        tmd->content_count[1] = content_count & 0xFF;
        memcpy(tmd->contentinfo[0].cmd_count, tmd->content_count, 2);
        if (FixCiaHeaderForTmd(&(cia->header), tmd) != 0) return 1;
    }
    
    // insert contents
    u8 titlekey[16] = { 0xFF };
    if ((GetTitleKey(titlekey, &(cia->ticket)) != 0) && force_legit) return 1;
    if (WriteCiaStub(cia, path_cia) != 0) return 1;
    for (u32 i = 0; (i < content_count) && (i < TMD_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(content_list[i]);
        snprintf(name_content, 256 - (name_content - path_content),
            (cdn) ? "%08lx" : (dlc && !cdn) ? "00000000/%08lx.app" : "%08lx.app", getbe32(chunk->id));
        if (InsertCiaContent(path_cia, path_content, 0, (u32) getbe64(chunk->size), chunk, titlekey, force_legit, false, cdn) != 0) {
            ShowPrompt(false, "ID %016llX.%08lX\nInsert content failed", getbe64(title_id), getbe32(chunk->id));
            return 1;
        }
    }
    
    // try to build & insert meta, but ignore result
    if (content_count && cdn) {
        if (!force_legit || !(getbe16(content_list->type) & 0x01)) {
            CiaInfo info;
            GetCiaInfo(&info, &(cia->header));
            if ((LoadNcchMeta(meta, path_cia, info.offset_content) == 0) && (InsertCiaMeta(path_cia, meta) == 0))
                cia->header.size_meta = CIA_META_SIZE;
        }
    } else if (content_count) {
        snprintf(name_content, 256 - (name_content - path_content), "%08lx.app", getbe32(content_list->id));
        if ((LoadNcchMeta(meta, path_content, 0) == 0) && (InsertCiaMeta(path_cia, meta) == 0))
            cia->header.size_meta = CIA_META_SIZE;
    }
    
    // write the CIA stub (take #2)
    if ((FixTmdHashes(tmd) != 0) || (WriteCiaStub(cia, path_cia) != 0))
        return 1;
    
    return 0;
}

u32 BuildCiaFromNcchFile(const char* path_ncch, const char* path_cia) {
    CiaStub* cia = (CiaStub*) TEMP_BUFFER;
    CiaMeta* meta = (CiaMeta*) (void*) (cia + 1);
    NcchExtHeader* exthdr = (NcchExtHeader*) (void*) (meta + 1);
    NcchHeader ncch;
    u8 title_id[8];
    u32 save_size = 0;
    
    // Init progress bar
    if (!ShowProgress(0, 0, path_ncch)) return 1;
    
    // load NCCH header / extheader, get save size && title id
    if (LoadNcchHeaders(&ncch, exthdr, NULL, path_ncch, 0) == 0) {
        save_size = getle32(exthdr->sys_info);
    } else {
        exthdr = NULL;
        if (LoadNcchHeaders(&ncch, NULL, NULL, path_ncch, 0) != 0) return 1;
    }
    for (u32 i = 0; i < 8; i++)
        title_id[i] = (ncch.programId >> ((7-i)*8)) & 0xFF;
    
    // build the CIA stub
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header)) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildFakeTicket(&(cia->ticket), title_id) != 0) ||
        (BuildFakeTmd(&(cia->tmd), title_id, 1, save_size)) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, path_cia) != 0)) {
        return 1;
    }
    
    // insert NCCH content
    TmdContentChunk* chunk = cia->content_list;
    memset(chunk, 0, sizeof(TmdContentChunk)); // nothing else to do
    if (InsertCiaContent(path_cia, path_ncch, 0, 0, chunk, NULL, false, true, false) != 0)
        return 1;
    
    // optional stuff (proper titlekey / meta data)
    FindTitleKey((&cia->ticket), title_id);
    if (exthdr && (BuildCiaMeta(meta, exthdr, NULL) == 0) &&
        (LoadExeFsFile(meta->smdh, path_ncch, 0, "icon", sizeof(meta->smdh), NULL) == 0) &&
        (InsertCiaMeta(path_cia, meta) == 0))
        cia->header.size_meta = CIA_META_SIZE;
    
    // write the CIA stub (take #2)
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, path_cia) != 0))
        return 1;
    
    return 0;
}

u32 BuildCiaFromNcsdFile(const char* path_ncsd, const char* path_cia) {
    CiaStub* cia = (CiaStub*) TEMP_BUFFER;
    CiaMeta* meta = (CiaMeta*) (void*) (cia + 1);
    NcchExtHeader* exthdr = (NcchExtHeader*) (void*) (meta + 1);
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
    if (LoadNcchHeaders(&ncch, exthdr, NULL, path_ncsd, NCSD_CNT0_OFFSET) != 0)
        return 1;
    save_size = getle32(exthdr->sys_info);
    
    // build the CIA stub
    memset(cia, 0, sizeof(CiaStub));
    if ((BuildCiaHeader(&(cia->header)) != 0) ||
        (BuildCiaCert(cia->cert) != 0) ||
        (BuildFakeTicket(&(cia->ticket), title_id) != 0) ||
        (BuildFakeTmd(&(cia->tmd), title_id, content_count, save_size)) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, path_cia) != 0)) {
        return 1;
    }
    
    // insert NCSD content
    TmdContentChunk* chunk = cia->content_list;
    for (u32 i = 0; i < 3; i++) {
        NcchPartition* partition = ncsd.partitions + i;
        u32 offset = partition->offset * NCSD_MEDIA_UNIT;
        u32 size = partition->size * NCSD_MEDIA_UNIT;
        if (!size) continue;
        memset(chunk, 0, sizeof(TmdContentChunk));
        chunk->id[3] = chunk->index[1] = i;
        if (InsertCiaContent(path_cia, path_ncsd, offset, size, chunk++, NULL, false, (i == 0), false) != 0)
            return 1;
    }
    
    // optional stuff (proper titlekey / meta data)
    FindTitleKey(&(cia->ticket), title_id);
    if ((BuildCiaMeta(meta, exthdr, NULL) == 0) &&
        (LoadExeFsFile(meta->smdh, path_ncsd, NCSD_CNT0_OFFSET, "icon", sizeof(meta->smdh), NULL) == 0) &&
        (InsertCiaMeta(path_cia, meta) == 0))
        cia->header.size_meta = CIA_META_SIZE;
    
    // write the CIA stub (take #2)
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (FixCiaHeaderForTmd(&(cia->header), &(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, path_cia) != 0))
        return 1;
    
    return 0;
}

u32 BuildCiaFromGameFile(const char* path, bool force_legit) {
    u64 filetype = IdentifyFileType(path);
    char dest[256];
    u32 ret = 0;
    
    // build output name
    snprintf(dest, 256, OUTPUT_PATH "/");
    char* dname = dest + strnlen(dest, 256);
    if (!((filetype & GAME_TMD) || (strncmp(path + 1, ":/title/", 8) == 0)) ||
        (GetGoodName(dname, path, false) != 0)) {
        char* name = strrchr(path, '/');
        if (!name) return 1;
        snprintf(dest, 256, "%s/%s", OUTPUT_PATH, ++name);
    }
    // replace extension
    char* dot = strrchr(dest, '.');
    if (!dot || (dot < strrchr(dest, '/')))
        dot = dest + strnlen(dest, 256);
    snprintf(dot, 16, ".%s", force_legit ? "legit.cia" : "cia");
        
    if (!CheckWritePermissions(dest)) return 1;
    f_unlink(dest); // remove the file if it already exists
    
    // ensure the output dir exists
    if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
        return 1;
    
    // build CIA from game file
    if (filetype & GAME_TMD)
        ret = BuildCiaFromTmdFile(path, dest, force_legit, filetype & FLAG_NUSCDN);
    else if (filetype & GAME_NCCH)
        ret = BuildCiaFromNcchFile(path, dest);
    else if (filetype & GAME_NCSD)
        ret = BuildCiaFromNcsdFile(path, dest);
    else ret = 1;
    
    if (ret != 0) // try to get rid of the borked file
        f_unlink(dest);
    
    return ret;
}

// this has very limited uses right now
u32 DumpCxiSrlFromTmdFile(const char* path) {
    u64 filetype = 0;
    char path_cxi[256];
    char dest[256];
    
    // prepare output name
    snprintf(dest, 256, OUTPUT_PATH "/");
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

u32 ExtractCodeFromCxiFile(const char* path, const char* path_out, char* extstr) {
    u8* code = (u8*) TEMP_BUFFER;
    u32 code_max_size = TEMP_BUFFER_EXTSIZE; // uses the extended temp buffer size
    
    NcchHeader ncch;
    NcchExtHeader exthdr;
    
    // load ncch, exthdr, .code
    u32 code_size;
    if ((LoadNcchHeaders(&ncch, &exthdr, NULL, path, 0) != 0) ||
        ((LoadExeFsFile(code, path, 0, EXEFS_CODE_NAME, code_max_size, &code_size) != 0) &&
         (LoadExeFsFile(code, path, 0, ".firm", code_max_size, &code_size) != 0)))
        return 1;
    
    // decompress code (only if required)
    if ((exthdr.flag & 0x1) && (DecompressCodeLzss(code, &code_size, code_max_size) != 0))
        return 1;
    
    // decide extension
    char* ext = EXEFS_CODE_NAME;
    if (code_size >= 0x200) {
        if (ValidateFirmHeader((FirmHeader*)(void*) code, code_size) == 0) ext = ".firm";
        else if (ValidateAgbHeader((AgbHeader*)(void*) code) == 0) ext = ".gba";
    }
    if (extstr) strncpy(extstr, ext, 7);
    
    // build or take over output path
    char dest[256];
    if (!path_out) {
        // ensure the output dir exists
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) return 1;
        snprintf(dest, 256, OUTPUT_PATH "/%016llX%s%s", ncch.programId, (exthdr.flag & 0x1) ? ".dec" : "", ext);
    } else strncpy(dest, path_out, 256);
    if (!CheckWritePermissions(dest)) return 1;
    
    // write output file
    fvx_unlink(dest);
    if (fvx_qwrite(dest, code, 0, code_size, NULL) != FR_OK) {
        fvx_unlink(dest);
        return 1;
    }
        
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

u32 ShowSmdhTitleInfo(Smdh* smdh) {
    const u32 lwrap = 24;
    u8* icon = (u8*) (TEMP_BUFFER + sizeof(Smdh));
    char* desc_l = (char*) icon + SMDH_SIZE_ICON_BIG;
    char* desc_s = (char*) desc_l + SMDH_SIZE_DESC_LONG;
    char* pub = (char*) desc_s + SMDH_SIZE_DESC_SHORT;
    if ((GetSmdhIconBig(icon, smdh) != 0) ||
        (GetSmdhDescLong(desc_l, smdh) != 0) ||
        (GetSmdhDescShort(desc_s, smdh) != 0) ||
        (GetSmdhPublisher(pub, smdh) != 0))
        return 1;
    WordWrapString(desc_l, lwrap);
    WordWrapString(desc_s, lwrap);
    WordWrapString(pub, lwrap);
    ShowIconString(icon, SMDH_DIM_ICON_BIG, SMDH_DIM_ICON_BIG, "%s\n%s\n%s", desc_l, desc_s, pub);
    InputWait(0);
    ClearScreenF(true, false, COLOR_STD_BG);
    return 0;
}

u32 ShowTwlIconTitleInfo(TwlIconData* twl_icon) {
    const u32 lwrap = 24;
    u8* icon = (u8*) (TEMP_BUFFER + sizeof(TwlIconData));
    char* desc = (char*) icon + TWLICON_SIZE_ICON;
    if ((GetTwlIcon(icon, twl_icon) != 0) ||
        (GetTwlTitle(desc, twl_icon) != 0))
        return 1;
    WordWrapString(desc, lwrap);
    ShowIconString(icon, TWLICON_DIM_ICON, TWLICON_DIM_ICON, "%s", desc);
    InputWait(0);
    ClearScreenF(true, false, COLOR_STD_BG);
    return 0;
}

u32 ShowGbaFileTitleInfo(const char* path) {
    AgbHeader agb;
    if ((fvx_qread(path, &agb, 0, sizeof(AgbHeader), NULL) != FR_OK) ||
        (ValidateAgbHeader(&agb) != 0)) return 1;
    ShowString("%.12s (AGB-%.4s)\n%s", agb.game_title, agb.game_code, AGB_DESTSTR(agb.game_code));
    InputWait(0);
    ClearScreenF(true, false, COLOR_STD_BG);
    return 0;
    
}

u32 ShowGameFileTitleInfo(const char* path) {
    Smdh* smdh = (Smdh*) (void*) TEMP_BUFFER;
    TwlIconData* twl_icon = (TwlIconData*) (void*) TEMP_BUFFER;
    
    char path_content[256];
    u64 itype = IdentifyFileType(path); // initial type
    if (itype & GAME_TMD) {
        if (GetTmdContentPath(path_content, path) != 0) return 1;
        path = path_content;
    }
    
    // try loading SMDH, then try NDS / GBA
    if (LoadSmdhFromGameFile(path, smdh) == 0)
        return ShowSmdhTitleInfo(smdh);
    else if ((LoadTwlMetaData(path, NULL, twl_icon) == 0) ||
        ((itype & GAME_TAD) && (fvx_qread(path, twl_icon, TAD_BANNER_OFFSET, sizeof(TwlIconData), NULL) == FR_OK)))
        return ShowTwlIconTitleInfo(twl_icon);
    else return ShowGbaFileTitleInfo(path);
}

u32 BuildNcchInfoXorpads(const char* destdir, const char* path) {
    FIL fp_info;
    FIL fp_xorpad;
    UINT bt;
    
    if (!CheckWritePermissions(destdir)) return 1;
    // warning: this will only build output dirs in the root dir (!!!)
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
    for (u32 i = 0; (i < info.n_entries) && (ret == 0); i++) {
        NcchInfoEntry entry;
        if ((fvx_read(&fp_info, &entry, entry_size, &bt) != FR_OK) ||
            (bt != entry_size)) ret = 1;
        if (FixNcchInfoEntry(&entry, version) != 0) ret = 1;
        if (ret != 0) break;
        
        char dest[256]; // 256 is the maximum length of a full path
        snprintf(dest, 256, "%s/%s", destdir, entry.filename);
        if (fvx_open(&fp_xorpad, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
            ret = 1;
        if (!ShowProgress(0, 0, entry.filename)) ret = 1;
        for (u64 p = 0; (p < entry.size_b) && (ret == 0); p += MAIN_BUFFER_SIZE) {
            UINT create_bytes = min(MAIN_BUFFER_SIZE, entry.size_b - p);
            if (BuildNcchInfoXorpad(MAIN_BUFFER, &entry, create_bytes, p) != 0) ret = 1;
            if (fvx_write(&fp_xorpad, MAIN_BUFFER, create_bytes, &bt) != FR_OK) ret = 1;
            if (!ShowProgress(p + create_bytes, entry.size_b, entry.filename)) ret = 1;
        }
        fvx_close(&fp_xorpad);
        if (ret != 0) f_unlink(dest); // get rid of the borked file
    }
    
    fvx_close(&fp_info);
    return ret;
}

u32 GetHealthAndSafetyPaths(const char* drv, char* path_cxi, char* path_bak) {
    const u32 tidlow_hs_o3ds[] = { 0x00020300, 0x00021300, 0x00022300, 0, 0x00026300, 0x00027300, 0x00028300 };
    const u32 tidlow_hs_n3ds[] = { 0x20020300, 0x20021300, 0x20022300, 0, 0, 0x20027300, 0 };
    
    // get H&S title id low
    u32 tidlow_hs = 0;
    for (char secchar = 'C'; secchar >= 'A'; secchar--) {
        char path_secinfo[32];
        u8 secinfo[0x111];
        u32 region = 0xFF;
        UINT br;
        snprintf(path_secinfo, 32, "%s/rw/sys/SecureInfo_%c", drv, secchar);
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
    for (u32 i = 0; i < 8; i++) { // 8 is an arbitrary number
        TitleMetaData* tmd = (TitleMetaData*) TEMP_BUFFER;
        TmdContentChunk* chunk = (TmdContentChunk*) (tmd + 1);
        char path_tmd[64];
        snprintf(path_tmd, 64, "%s/title/00040010/%08lx/content/%08lx.tmd", drv, tidlow_hs, i);
        if (LoadTmdFile(tmd, path_tmd) != 0) continue;
        if (!getbe16(tmd->content_count)) return 1;
        if (path_cxi) snprintf(path_cxi, 64, "%s/title/00040010/%08lx/content/%08lx.app", drv, tidlow_hs, getbe32(chunk->id));
        if (path_bak) snprintf(path_bak, 64, "%s/title/00040010/%08lx/content/%08lx.bak", drv, tidlow_hs, getbe32(chunk->id));
        break;
    }
    
    return ((path_cxi && !*path_cxi) || (path_bak && !*path_bak)) ? 1 : 0;
}

u32 CheckHealthAndSafetyInject(const char* hsdrv) {
    char path_bak[64] = { 0 };
    return ((GetHealthAndSafetyPaths(hsdrv, NULL, path_bak) == 0) &&
        (f_stat(path_bak, NULL) == FR_OK)) ? 0 : 1;
}

u32 InjectHealthAndSafety(const char* path, const char* destdrv) {
    NcchHeader ncch;
        
    // write permissions
    if (!CheckWritePermissions(destdrv))
        return 1;
    
    // legacy stuff - remove mark file
    char path_mrk[32] = { 0 };
    snprintf(path_mrk, 32, "%s/%s", destdrv, "__gm9_hsbak.pth");
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
    if ((LoadNcchHeaders(&ncch, NULL, NULL, path, 0) != 0) ||
        !(NCCH_IS_CXI(&ncch)) || (SetupNcchCrypto(&ncch, NCCH_NOCRYPTO) != 0))
        return 1;
    
    // check crypto, get sig
    if ((LoadNcchHeaders(&ncch, NULL, NULL, path_cxi, 0) != 0) ||
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
    
    // fix up the injected H&S NCCH header (copy H&S signature, title ID) 
    if ((ret == 0) && (LoadNcchHeaders(&ncch, NULL, NULL, path_cxi, 0) == 0)) {
        UINT bw;
        ncch.programId = tid_hs;
        ncch.partitionId = tid_hs;
        memcpy(ncch.signature, sig, 0x100);
        if ((fvx_qwrite(path_cxi, &ncch, 0, sizeof(NcchHeader), &bw) != FR_OK) ||
            (bw != sizeof(NcchHeader)))
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
    TitleKeysInfo* tik_info = (TitleKeysInfo*) MAIN_BUFFER;
    const char* path_out = (dec) ? OUTPUT_PATH "/" TIKDB_NAME_DEC : OUTPUT_PATH "/" TIKDB_NAME_ENC;
    const char* path_in = path;
    UINT br;
    
    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;
    
    if (!path_in && !dump) { // no input path given - initialize
        memset(tik_info, 0, 16);
        if ((fvx_stat(path_out, NULL) == FR_OK) &&
            (ShowPrompt(true, "%s\nOutput file already exists.\nUpdate this?", path_out)))
            path_in = path_out;
        else return 0;
    }
    
    u64 filetype = path_in ? IdentifyFileType(path_in) : 0;
    if (filetype & GAME_TICKET) {
        Ticket* ticket = (Ticket*) TEMP_BUFFER;
        if ((fvx_qread(path_in, ticket, 0, TICKET_SIZE, &br) != FR_OK) || (br != TICKET_SIZE) ||
            (TIKDB_SIZE(tik_info) + 32 > MAIN_BUFFER_SIZE) || (AddTicketToInfo(tik_info, ticket, dec) != 0)) return 1;
    } else if (filetype & SYS_TICKDB) {
        const u32 area_offsets[] = { TICKDB_AREA_OFFSETS };
        FIL file;
        if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) return 1;
        // parse file, sector by sector
        for (u32 p = 0; p < sizeof(area_offsets) / sizeof(u32); p++) {
            fvx_lseek(&file, area_offsets[p]);
            fvx_sync(&file);
            for (u32 i = 0; i < TICKDB_AREA_SIZE; i += (TEMP_BUFFER_SIZE - 0x200)) {
                u32 read_bytes = min(TEMP_BUFFER_SIZE, TICKDB_AREA_SIZE - i);
                u8* data = (u8*) TEMP_BUFFER;
                if ((fvx_read(&file, data, read_bytes, &br) != FR_OK) || (br != read_bytes)) {
                    fvx_close(&file);
                    return 1;
                }
                for (; data + TICKET_SIZE < ((u8*) TEMP_BUFFER) + read_bytes; data += 0x200) {
                    Ticket* ticket = TicketFromTickDbChunk(data, NULL, false);
                    if (!ticket || (ticket->commonkey_idx >= 2) || !getbe64(ticket->ticket_id)) continue;
                    if (TIKDB_SIZE(tik_info) + 32 > MAIN_BUFFER_SIZE) return 1;
                    AddTicketToInfo(tik_info, ticket, dec); // ignore result
                }
            }
        }
        fvx_close(&file);
    } else if (filetype & BIN_TIKDB) {
        TitleKeysInfo* tik_info_merge = (TitleKeysInfo*) TEMP_BUFFER;
        if ((fvx_qread(path_in, tik_info_merge, 0, TEMP_BUFFER_SIZE, &br) != FR_OK) ||
            (TIKDB_SIZE(tik_info_merge) != br)) return 1;
        // merge and rebuild TitleKeyInfo
        u32 n_entries = tik_info_merge->n_entries;
        TitleKeyEntry* tik = tik_info_merge->entries;
        for (u32 i = 0; i < n_entries; i++, tik++) {
            if (TIKDB_SIZE(tik_info) + 32 > MAIN_BUFFER_SIZE) return 1;
            AddTitleKeyToInfo(tik_info, tik, !(filetype & FLAG_ENC), dec, false); // ignore result
                
        }
    }
    
    if (dump) {
        u32 dump_size = TIKDB_SIZE(tik_info);
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) // ensure the output dir exists
            return 1;
        f_unlink(path_out);
        if ((dump_size <= 16) || (fvx_qwrite(path_out, tik_info, 0, dump_size, &br) != FR_OK) || (br != dump_size))
            return 1;
    }
    
    return 0;
}

u32 BuildSeedInfo(const char* path, bool dump) {
    SeedInfo* seed_info = (SeedInfo*) MAIN_BUFFER;
    const char* path_out = OUTPUT_PATH "/" SEEDDB_NAME;
    const char* path_in = path;
    u32 inputtype = 0; // 0 -> none, 1 -> seeddb.bin, 2 -> seed system save
    UINT br;
    
    // write permissions
    if (!CheckWritePermissions(path_out))
        return 1;
    
    if (!path_in && !dump) { // no input path given - initialize
        memset(seed_info, 0, 16);
        if ((fvx_stat(path_out, NULL) == FR_OK) &&
            (ShowPrompt(true, "%s\nOutput file already exists.\nUpdate this?", path_out))) {
            path_in = path_out;
            inputtype = 1;
        } else return 0;
    }
    
    char path_str[128];
    if (path_in && (strnlen(path_in, 16) == 2)) { // when only a drive is given...
        // grab the key Y from movable.sed
        u8 movable_keyy[16];
        snprintf(path_str, 128, "%s/private/movable.sed", path_in);
        if ((fvx_qread(path_str, movable_keyy, 0x110, 0x10, &br) != FR_OK) || (br != 0x10))
            return 1;
        // build the seed save path
        u32 sha256sum[8];
        sha_quick(sha256sum, movable_keyy, 0x10, SHA256_MODE);
        snprintf(path_str, 128, "%s/data/%08lX%08lX%08lX%08lX/sysdata/0001000F/00000000",
            path_in, sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
        path_in = path_str;
        inputtype = 2;
    }
    
    if (inputtype == 1) { // seeddb.bin input
        SeedInfo* seed_info_merge = (SeedInfo*) TEMP_BUFFER;
        if ((fvx_qread(path_in, seed_info_merge, 0, TEMP_BUFFER_SIZE, &br) != FR_OK) ||
            (SEEDDB_SIZE(seed_info_merge) != br)) return 1;
        // merge and rebuild SeedInfo
        u32 n_entries = seed_info_merge->n_entries;
        SeedInfoEntry* seed = seed_info_merge->entries;
        for (u32 i = 0; i < n_entries; i++, seed++) {
            if (SEEDDB_SIZE(seed_info) + 32 > MAIN_BUFFER_SIZE) return 1;
            AddSeedToDb(seed_info, seed); // ignore result        
        }
    } else if (inputtype == 2) { // seed system save input
        static const u32 seed_offset[2] = {SEEDSAVE_AREA_OFFSETS};
        u8* seedsave = (u8*) TEMP_BUFFER;
        if ((fvx_qread(path_in, seedsave, 0, 0x200, &br) != FR_OK) || (br != 0x200))
            return 1;
        u32 p_active = (getle32(seedsave + 0x168)) ? 1 : 0;
        for (u32 p = 0; p < 2; p++) {
            SeedInfoEntry seed = { 0 };
            if ((fvx_qread(path_in, seedsave, seed_offset[(p + p_active) % 2], SEEDSAVE_MAX_ENTRIES*(8+16), &br) != FR_OK) ||
                (br != SEEDSAVE_MAX_ENTRIES*(8+16)))
                return 1;
            for (u32 s = 0; s < SEEDSAVE_MAX_ENTRIES; s++) {
                seed.titleId = getle64(seedsave + (s*8));
                memcpy(seed.seed, seedsave + (SEEDSAVE_MAX_ENTRIES*8) + (s*16), 16);
                if (((seed.titleId >> 32) != 0x00040000) ||
                    (!getle64(seed.seed) && !getle64(seed.seed + 8))) continue;
                if (SEEDDB_SIZE(seed_info) + 32 > MAIN_BUFFER_SIZE) return 1;
                AddSeedToDb(seed_info, &seed); // ignore result 
            }
        }
    }
    
    if (dump) {
        u32 dump_size = SEEDDB_SIZE(seed_info);
        if (fvx_rmkdir(OUTPUT_PATH) != FR_OK) // ensure the output dir exists
            return 1;
        f_unlink(path_out);
        if ((dump_size <= 16) || (fvx_qwrite(path_out, seed_info, 0, dump_size, &br) != FR_OK) || (br != dump_size))
            return 1;
    }
    
    return 0;
}

u32 LoadNcchFromGameFile(const char* path, NcchHeader* ncch) {
    u64 filetype = IdentifyFileType(path);
    UINT br;
    
    if (filetype & GAME_NCCH) {
        if ((fvx_qread(path, ncch, 0, sizeof(NcchHeader), &br) == FR_OK) &&
            (br == sizeof(NcchHeader)) && (ValidateNcchHeader(ncch) == 0)) return 0;
    } else if (filetype & GAME_NCSD) {
        if ((fvx_qread(path, ncch, NCSD_CNT0_OFFSET, sizeof(NcchHeader), &br) == FR_OK) &&
            (br == sizeof(NcchHeader)) && (ValidateNcchHeader(ncch) == 0)) return 0;
    } else if (filetype & GAME_CIA) {
        CiaStub* cia = (CiaStub*) TEMP_BUFFER;
        CiaInfo info;
        
        // load CIA stub from path
        if ((LoadCiaStub(cia, path) != 0) ||
            (GetCiaInfo(&info, &(cia->header)) != 0)) {
            return 1;
        }
        
        // decrypt / load NCCH header from first CIA content
        if (getbe16(cia->tmd.content_count)) {
            TmdContentChunk* chunk = cia->content_list;
            if ((getbe64(chunk->size) < sizeof(NcchHeader)) ||
                (fvx_qread(path, ncch, info.offset_content, sizeof(NcchHeader), &br) != FR_OK) ||
                (br != sizeof(NcchHeader))) return 1;
            if (getbe16(chunk->type) & 0x1) { // decrypt first content header
                u8 titlekey[16];
                u8 ctr[16];
                GetTmdCtr(ctr, chunk);
                if (GetTitleKey(titlekey, &(cia->ticket)) != 0) return 1;
                DecryptCiaContentSequential((void*) ncch, sizeof(NcchHeader), ctr, titlekey);
            }
            if (ValidateNcchHeader(ncch) == 0) return 0;
        }
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
        (type_donor & GAME_TMD)  ? "tmd" : "";
    if (!*ext) return 1;
    
    char appid_str[1 + 8 + 1] = { 0 }; // handling for NCCH / NDS in "?:/title" paths
    if ((type_donor & (GAME_NCCH|GAME_NDS)) && (strncmp(path + 1, ":/title/", 8) == 0)) {
        char* name = strrchr(path, '/');
        if (name && (strnlen(++name, 16) >= 8))
            *appid_str = '.';
        strncpy(appid_str + 1, name, 8);
    }
    
    char path_content[256];
    if (type_donor & GAME_TMD) {
        if (GetTmdContentPath(path_content, path) != 0) return 1;
        path_donor = path_content;
        type_donor = IdentifyFileType(path_donor);
    }
    
    if (type_donor & GAME_GBA) { // AGB
        AgbHeader* agb = (AgbHeader*) TEMP_BUFFER;
        if (fvx_qread(path_donor, agb, 0, sizeof(AgbHeader), NULL) != FR_OK) return 1;
        snprintf(name, 128, "%.12s (AGB-%.4s).%s", agb->game_title, agb->game_code, ext);
    } else if (type_donor & GAME_NDS) { // NTR or TWL
        TwlHeader* twl = (TwlHeader*) TEMP_BUFFER;
        TwlIconData* icon = (TwlIconData*) (TEMP_BUFFER + sizeof(TwlHeader));
        if (LoadTwlMetaData(path_donor, twl, quick ? NULL : icon) != 0) return 1;
        if (quick) {
            if (twl->unit_code & 0x02) { // TWL
                snprintf(name, 128, "%016llX (TWL-%.4s).%s", twl->title_id, twl->game_code, ext);
            } else { // NTR
                snprintf(name, 128, "%.12s (NTR-%.4s).%s", twl->game_title, twl->game_code, ext);
            }
        } else {
            char title_name[0x80+1] = { 0 };
            if (GetTwlTitle(title_name, icon) != 0) return 1;
            char* linebrk = strchr(title_name, '\n');
            if (linebrk) *linebrk = '\0';
            
            if (twl->unit_code & 0x02) { // TWL
                char region[8] = { 0 };
                if (twl->region_flags == TWL_REGION_FREE) snprintf(region, 8, "W");
                snprintf(region, 8, "%s%s%s%s%s",
                    (twl->region_flags & REGION_MASK_JPN) ? "J" : "",
                    (twl->region_flags & REGION_MASK_USA) ? "U" : "",
                    (twl->region_flags & REGION_MASK_EUR) ? "E" : "",
                    (twl->region_flags & REGION_MASK_CHN) ? "C" : "",
                    (twl->region_flags & REGION_MASK_KOR) ? "K" : "");
                if (strncmp(region, "JUECK", 8) == 0) snprintf(region, 8, "W");
                if (!*region) snprintf(region, 8, "UNK");
                
                char* unit_str = (twl->unit_code == TWL_UNITCODE_TWLNTR) ? "DSi Enhanced" : "DSi Exclusive";
                snprintf(name, 128, "%016llX %s (TWL-%.4s) (%s) (%s).%s",
                    twl->title_id, title_name, twl->game_code, unit_str, region, ext);
            } else { // NTR
                snprintf(name, 128, "%s (NTR-%.4s).%s", title_name, twl->game_code, ext);
            }
        }
    } else if (type_donor & (GAME_CIA|GAME_NCSD|GAME_NCCH)) { // CTR (data from NCCH)
        NcchHeader* ncch = (NcchHeader*) (void*) TEMP_BUFFER;
        Smdh* smdh = (Smdh*) (TEMP_BUFFER + sizeof(NcchHeader));
        if (LoadNcchFromGameFile(path_donor, ncch) != 0) return 1;
        if (quick || (LoadSmdhFromGameFile(path_donor, smdh) != 0)) {
            snprintf(name, 128, "%016llX%s (%.16s).%s", ncch->programId, appid_str, ncch->productcode, ext);
        } else {
            char title_name[0x40+1] = { 0 };
            if (GetSmdhDescShort(title_name, smdh) != 0) return 1;
            
            char region[8] = { 0 };
            if (smdh->region_lockout == SMDH_REGION_FREE) snprintf(region, 8, "W");
            snprintf(region, 8, "%s%s%s%s%s%s",
                (smdh->region_lockout & REGION_MASK_JPN) ? "J" : "",
                (smdh->region_lockout & REGION_MASK_USA) ? "U" : "",
                (smdh->region_lockout & REGION_MASK_EUR) ? "E" : "",
                (smdh->region_lockout & REGION_MASK_CHN) ? "C" : "",
                (smdh->region_lockout & REGION_MASK_KOR) ? "K" : "",
                (smdh->region_lockout & REGION_MASK_TWN) ? "T" : "");
            if (strncmp(region, "JUECKT", 8) == 0) snprintf(region, 8, "W");
            if (!*region) snprintf(region, 8, "UNK");
            
            snprintf(name, 128, "%016llX%s %s (%.16s) (%s).%s",
                ncch->programId, appid_str, title_name, ncch->productcode, region, ext);
        }
    } else return 1;
    
    // remove illegal chars from filename
    for (char* c = name; *c; c++) {
        if ((*c == ':') || (*c == '/') || (*c == '\\') || (*c == '"') ||
            (*c == '*') || (*c == '?') || (*c == '\n') || (*c == '\r'))
            *c = ' ';
    }
    
    // remove double spaces from filename
    char* s = name;
    for (char* c = name; *s; c++, s++) {
        while ((*c == ' ') && (*(c+1) == ' ')) c++;
        *s = *c;
    }
    
    return 0;
}
