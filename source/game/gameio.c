#include "gameio.h"
#include "game.h"
#include "ui.h"
#include "filetype.h"
#include "sddata.h"
#include "aes.h"
#include "sha.h"
#include "ff.h"

u32 GetNcchFileHeaders(NcchHeader* ncch, ExeFsHeader* exefs, FIL* file) {
    u32 offset_ncch = f_tell(file);
    UINT btr;
    
    if ((fx_read(file, ncch, sizeof(NcchHeader), &btr) != FR_OK) ||
        (ValidateNcchHeader(ncch) != 0))
        return 1;
    
    if (exefs && ncch->size_exefs) {
        u32 offset_exefs = offset_ncch + (ncch->offset_exefs * NCCH_MEDIA_UNIT);
        f_lseek(file, offset_exefs);
        if ((fx_read(file, exefs, sizeof(ExeFsHeader), &btr) != FR_OK) ||
            (DecryptNcch((u8*) exefs, ncch->offset_exefs * NCCH_MEDIA_UNIT, sizeof(ExeFsHeader), ncch, NULL) != 0) ||
            (ValidateExeFsHeader(exefs, ncch->size_exefs * NCCH_MEDIA_UNIT) != 0))
            return 1;
    }
    
    return 0;
}

u32 CheckNcchFileHash(u8* expected, FIL* file, u32 size_data, u32 offset_ncch, NcchHeader* ncch, ExeFsHeader* exefs) {
    u32 offset_data = f_tell(file) - offset_ncch;
    u8 hash[32];
    
    sha_init(SHA256_MODE);
    for (u32 i = 0; i < size_data; i += MAIN_BUFFER_SIZE) {
        u32 read_bytes = min(MAIN_BUFFER_SIZE, (size_data - i));
        UINT bytes_read;
        fx_read(file, MAIN_BUFFER, read_bytes, &bytes_read);
        DecryptNcch(MAIN_BUFFER, offset_data + i, read_bytes, ncch, exefs);
        sha_update(MAIN_BUFFER, read_bytes);
    }
    sha_get(hash);
    
    return (memcmp(hash, expected, 32) == 0) ? 0 : 1;
}

u32 VerifyNcchFile(const char* path, u32 offset, u32 size) {
    NcchHeader ncch;
    ExeFsHeader exefs;
    FIL file;
    
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // open file, get NCCH, ExeFS header
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    f_lseek(&file, offset);
    
    if (GetNcchFileHeaders(&ncch, &exefs, &file) != 0) {
        if (!offset) ShowPrompt(false, "%s\nError: Not a NCCH file", pathstr);
        fx_close(&file);
        return 1;
    }
    
    // size checks
    if (!size) size = f_size(&file) - offset;
    if ((f_size(&file) < offset) || (size < ncch.size * NCCH_MEDIA_UNIT)) {
        if (!offset) ShowPrompt(false, "%s\nError: File is too small", pathstr);
        fx_close(&file);
        return 1;
    }
    
    // check / setup crypto
    if (SetupNcchCrypto(&ncch) != 0) {
        if (!offset) ShowPrompt(false, "%s\nError: Crypto not set up", pathstr);
        fx_close(&file);
        return 1;
    }
    
    u32 ver_exthdr = 0;
    u32 ver_exefs = 0;
    u32 ver_romfs = 0;
    
    // base hash check for extheader
    if (ncch.size_exthdr > 0) {
        f_lseek(&file, offset + NCCH_EXTHDR_OFFSET);
        ver_exthdr = CheckNcchFileHash(ncch.hash_exthdr, &file, 0x400, offset, &ncch, &exefs);
    }
    
    // base hash check for exefs
    if (ncch.size_exefs > 0) {
        f_lseek(&file, offset + (ncch.offset_exefs * NCCH_MEDIA_UNIT));
        ver_exefs = CheckNcchFileHash(ncch.hash_exefs, &file, ncch.size_exefs_hash * NCCH_MEDIA_UNIT, offset, &ncch, &exefs);
    }
    
    // base hash check for romfs
    if (ncch.size_romfs > 0) {
        f_lseek(&file, offset + (ncch.offset_romfs * NCCH_MEDIA_UNIT));
        ver_romfs = CheckNcchFileHash(ncch.hash_romfs, &file, ncch.size_romfs_hash * NCCH_MEDIA_UNIT, offset, &ncch, &exefs);
    }
    
    // thorough exefs verification
    if (ncch.size_exefs > 0) {
        for (u32 i = 0; !ver_exefs && (i < 10); i++) {
            ExeFsFileHeader* exefile = exefs.files + i;
            u8* hash = exefs.hashes[9 - i];
            if (!exefile->size) continue;
            f_lseek(&file, offset + (ncch.offset_exefs * NCCH_MEDIA_UNIT) + 0x200 + exefile->offset);
            ver_exefs = CheckNcchFileHash(hash, &file, exefile->size, offset, &ncch, &exefs);
        }
    }
    
    if (!offset && (ver_exthdr|ver_exefs|ver_romfs)) { // verification summary
        ShowPrompt(false, "%s\nNCCH verification failed:\nExtHdr/ExeFS/RomFS: %s/%s/%s", pathstr,
            (!ncch.size_exthdr) ? "-" : (ver_exthdr == 0) ? "ok" : "fail",
            (!ncch.size_exefs) ? "-" : (ver_exefs == 0) ? "ok" : "fail",
            (!ncch.size_romfs) ? "-" : (ver_romfs == 0) ? "ok" : "fail");
    }
    
    fx_close(&file);
    return ver_exthdr|ver_exefs|ver_romfs;
}

u32 LoadNcsdHeader(NcsdHeader* ncsd, const char* path) {
    FIL file;
    UINT btr;
    
    // open file, get NCSD header
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    f_lseek(&file, 0);
    if ((fx_read(&file, ncsd, sizeof(NcsdHeader), &btr) != FR_OK) ||
        (ValidateNcsdHeader(ncsd) != 0)) {
        fx_close(&file);
        return 1;
    }
    fx_close(&file);
    
    return 0;
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

u32 LoadCiaStub(CiaStub* stub, const char* path) {
    FIL file;
    UINT btr;
    CiaInfo info;
    
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    // first 0x20 byte of CIA header
    f_lseek(&file, 0);
    if ((fx_read(&file, stub, 0x20, &btr) != FR_OK) || (btr != 0x20) ||
        (ValidateCiaHeader(&(stub->header)) != 0)) {
        fx_close(&file);
        return 1;
    }
    GetCiaInfo(&info, &(stub->header));
    
    // everything up till content offset
    f_lseek(&file, 0);
    if ((fx_read(&file, stub, info.offset_content, &btr) != FR_OK) || (btr != info.offset_content)) {
        fx_close(&file);
        return 1;
    }
    
    fx_close(&file);
    return 0;
}

u32 LoadTmdFile(TitleMetaData* tmd, const char* path) {
    FIL file;
    UINT btr;
    
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    // full TMD file
    f_lseek(&file, 0);
    if ((fx_read(&file, tmd, CIA_TMD_SIZE_MAX, &btr) != FR_OK) ||
        (btr < CIA_TMD_SIZE_N(getbe16(tmd->content_count)))) {
        fx_close(&file);
        return 1;
    }
    
    fx_close(&file);
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
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    if (offset + size > f_size(&file)) {
        fx_close(&file);
        return 1;
    }
    f_lseek(&file, offset);
    
    GetTmdCtr(ctr, chunk);
    sha_init(SHA256_MODE);
    for (u32 i = 0; i < size; i += MAIN_BUFFER_SIZE) {
        u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
        UINT bytes_read;
        fx_read(&file, MAIN_BUFFER, read_bytes, &bytes_read);
        if (encrypted) DecryptCiaContent(MAIN_BUFFER, read_bytes, ctr, titlekey);
        sha_update(MAIN_BUFFER, read_bytes);
        if (!ShowProgress(i + read_bytes, size, path)) break;
    }
    sha_get(hash);
    fx_close(&file);
    
    return memcmp(hash, expected, 32);
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
    for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
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

u32 VerifyTmdFile(const char* path) {
    TitleMetaData* tmd = (TitleMetaData*) TEMP_BUFFER;
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    
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
    
    // verify contents
    u32 content_count = getbe16(tmd->content_count);
    for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(content_list[i]);
        chunk->type[1] &= ~0x01; // remove crypto flag
        snprintf(name_content, 256 - (name_content - path_content), "%08lx.app", getbe32(chunk->id));
        TruncateString(pathstr, path_content, 32, 8);
        if (VerifyTmdContent(path_content, 0, chunk, NULL) != 0) {
            ShowPrompt(false, "%s\nVerification failed", pathstr);
            return 1;
        }
    }
    
    return 0;
}

u32 VerifyGameFile(const char* path) {
    u32 filetype = IdentifyFileType(path);
    if (filetype == GAME_CIA)
        return VerifyCiaFile(path);
    else if (filetype == GAME_NCSD)
        return VerifyNcsdFile(path);
    else if (filetype == GAME_NCCH)
        return VerifyNcchFile(path, 0, 0);
    else if (filetype == GAME_TMD)
        return VerifyTmdFile(path);
    else return 1;
}
