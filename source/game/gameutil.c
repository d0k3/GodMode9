#include "gameutil.h"
#include "game.h"
#include "ui.h"
#include "fsperm.h"
#include "filetype.h"
#include "sddata.h"
#include "aes.h"
#include "sha.h"
#include "ff.h"

u32 GetOutputPath(char* dest, const char* path, const char* ext) {
    // special handling for input from title directories (somewhat hacky)
    if ((strspn(path, "AB147") > 0) && (strncmp(path + 1, ":/title/", 8) == 0)) {
        u32 tid_high, tid_low, app_id;
        char drv;
        if ((sscanf(path, "%c:/title/%08lx/%08lx/content/%08lx", &drv, &tid_high, &tid_low, &app_id) == 4) &&
            (strnlen(path, 256) == (1+1+1) + (5+1) + (8+1) + (8+1) + (7+1) + (8+1+3))) { // confused? ^_^
            if (!ext) snprintf(dest, 256, "%s/%08lx%08lx.%08lx.app", OUTPUT_PATH, tid_high, tid_low, app_id);
            else snprintf(dest, 256, "%s/%08lx%08lx.%s", OUTPUT_PATH, tid_high, tid_low, ext);
            return 0;
        }
    }
    
    // handling for everything else
    char* name = strrchr(path, '/');
    if (!name) return 1;
    snprintf(dest, 256, "%s/%s", OUTPUT_PATH, ++name);
    if (ext) { // replace extension
        char* dot = strrchr(dest, '.');
        if (!dot || ((dot - dest) <= (int) strnlen(OUTPUT_PATH, 256) + 1))
            dot = dest + strnlen(dest, 256);
        snprintf(dot, 8, ".%s", ext);
    }
    
    return 0;
}

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

u32 WriteCiaStub(CiaStub* stub, const char* path) {
    FIL file;
    UINT btw;
    CiaInfo info;
    
    GetCiaInfo(&info, &(stub->header));
    
    // everything up till content offset
    if (fx_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
        return 1;
    f_lseek(&file, 0);
    if ((fx_write(&file, stub, info.offset_content, &btw) != FR_OK) || (btw != info.offset_content)) {
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
        if (encrypted) DecryptCiaContentSequential(MAIN_BUFFER, read_bytes, ctr, titlekey);
        sha_update(MAIN_BUFFER, read_bytes);
        if (!ShowProgress(i + read_bytes, size, path)) break;
    }
    sha_get(hash);
    fx_close(&file);
    
    return memcmp(hash, expected, 32);
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

u32 CheckEncryptedNcchFile(const char* path, u32 offset) {
    NcchHeader ncch;
    FIL file;
    UINT btr;
    
    // open file, get NCCH header
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    f_lseek(&file, offset);
    if ((fx_read(&file, &ncch, sizeof(NcchHeader), &btr) != FR_OK) ||
        (ValidateNcchHeader(&ncch) != 0)) {
        fx_close(&file);
        return 1;
    }
    fx_close(&file);
    
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
    for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        if ((getbe16(chunk->type) & 0x1) || (CheckEncryptedNcchFile(path, next_offset)))
            return 0; // encryption found
        next_offset += getbe64(chunk->size);
    }
    
    return 1;
}

u32 CheckEncryptedGameFile(const char* path) {
    u32 filetype = IdentifyFileType(path);
    if (filetype == GAME_CIA)
        return CheckEncryptedCiaFile(path);
    else if (filetype == GAME_NCSD)
        return CheckEncryptedNcsdFile(path);
    else if (filetype == GAME_NCCH)
        return CheckEncryptedNcchFile(path, 0);
    else return 1;
}

u32 DecryptNcchNcsdFile(const char* orig, const char* dest, u32 mode,
    u32 offset, u32 size, TmdContentChunk* chunk, const u8* titlekey) { // this line only for CIA contents
    // this will do a simple copy for unencrypted files
    bool inplace = (strncmp(orig, dest, 256) == 0);
    FIL ofile;
    FIL dfile;
    FIL* ofp = &ofile;
    FIL* dfp = (inplace) ? &ofile : &dfile;
    FSIZE_t fsize;
    
    // open file(s)
    if (inplace) {
        if (fx_open(ofp, orig, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
            return 1;
        f_lseek(ofp, offset);
    } else {
        if (fx_open(ofp, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return 1;
        if (fx_open(dfp, dest, FA_WRITE | (offset ? FA_OPEN_ALWAYS : FA_CREATE_ALWAYS)) != FR_OK) {
            fx_close(ofp);
            return 1;
        }
        f_lseek(ofp, offset);
        f_lseek(dfp, offset);
    }
    
    fsize = f_size(ofp); // for progress bar
    if (!size) size = fsize;
    
    u32 ret = 0;
    if (mode & (GAME_NCCH|GAME_NCSD)) { // for NCCH / NCSD files
        if (!ShowProgress(offset, fsize, dest)) ret = 1;
        for (u32 i = 0; (i < size) && (ret == 0); i += MAIN_BUFFER_SIZE) {
            u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
            UINT bytes_read, bytes_written;
            if (fx_read(ofp, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK) ret = 1;
            if (((mode & GAME_NCCH) && (DecryptNcchSequential(MAIN_BUFFER, i, read_bytes) != 0)) ||
                ((mode & GAME_NCSD) && (DecryptNcsdSequential(MAIN_BUFFER, i, read_bytes) != 0)))
                ret = 1;
            if (inplace) f_lseek(ofp, f_tell(ofp) - read_bytes);
            if (fx_write(dfp, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK) ret = 1;
            if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
            if (!ShowProgress(offset + i + read_bytes, fsize, dest)) ret = 1;
        }
    } else if (mode & GAME_CIA) { // for NCCHs inside CIAs
        if (!ShowProgress(offset, fsize, dest)) ret = 1;
        bool cia_crypto = getbe16(chunk->type) & 0x1;
        bool ncch_crypto; // find out by decrypting the NCCH header
        UINT bytes_read, bytes_written;
        u8 ctr[16];
        
        GetTmdCtr(ctr, chunk); // NCCH crypto?
        if (fx_read(ofp, MAIN_BUFFER, sizeof(NcchHeader), &bytes_read) != FR_OK) ret = 1;
        if (cia_crypto) DecryptCiaContentSequential(MAIN_BUFFER, sizeof(NcchHeader), ctr, titlekey);
        ncch_crypto = ((ValidateNcchHeader((NcchHeader*) (void*) MAIN_BUFFER) == 0) &&
            NCCH_ENCRYPTED((NcchHeader*) (void*) MAIN_BUFFER));
        if (ncch_crypto && (SetupNcchCrypto((NcchHeader*) (void*) MAIN_BUFFER) != 0)) {
            ShowPrompt(false, "Error: Cannot set up NCCH crypto");
        };
        
        GetTmdCtr(ctr, chunk);
        f_lseek(ofp, offset);
        sha_init(SHA256_MODE);
        for (u32 i = 0; (i < size) && (ret == 0); i += MAIN_BUFFER_SIZE) {
            u32 read_bytes = min(MAIN_BUFFER_SIZE, (size - i));
            if (fx_read(ofp, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK) ret = 1;
            if (cia_crypto && (DecryptCiaContentSequential(MAIN_BUFFER, read_bytes, ctr, titlekey) != 0)) ret = 1;
            if (ncch_crypto && (DecryptNcchSequential(MAIN_BUFFER, i, read_bytes) != 0)) ret = 1;
            if (inplace) f_lseek(ofp, f_tell(ofp) - read_bytes);
            if (fx_write(dfp, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK) ret = 1;
            sha_update(MAIN_BUFFER, read_bytes);
            if ((read_bytes != bytes_read) || (bytes_read != bytes_written)) ret = 1;
            if (!ShowProgress(offset + i + read_bytes, fsize, dest)) ret = 1;
        }
        sha_get(chunk->hash);
        chunk->type[1] &= ~0x01;
    }
    
    fx_close(ofp);
    if (!inplace) fx_close(dfp);
    
    return ret;
}

u32 DecryptCiaFile(const char* orig, const char* dest) {
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
    for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
        TmdContentChunk* chunk = &(cia->content_list[i]);
        u64 size = getbe64(chunk->size);
        if (DecryptNcchNcsdFile(orig, dest, GAME_CIA, next_offset, size, chunk, titlekey) != 0)
            return 1;
        next_offset += size;
    }
    
    // fix TMD hashes, write CIA stub to destination
    if ((FixTmdHashes(&(cia->tmd)) != 0) ||
        (WriteCiaStub(cia, dest) != 0)) return 1;
    
    return 0;
}

u32 DecryptGameFile(const char* path, bool inplace) {
    u32 filetype = IdentifyFileType(path);
    char dest[256];
    char* destptr = (char*) path;
    u32 ret = 0;
    
    if (!inplace) {
        if (GetOutputPath(dest, path, NULL) != 0) return 1;
        destptr = dest;
    }
    
    if (!CheckWritePermissions(destptr))
        return 1;
    
    if (!inplace) {
        // ensure the output dir exists
        if ((f_stat(OUTPUT_PATH, NULL) != FR_OK) && (f_mkdir(OUTPUT_PATH) != FR_OK))
            return 1;
    }
    
    if (filetype & GAME_CIA)
        ret = DecryptCiaFile(path, destptr);
    else if (filetype & (GAME_NCCH|GAME_NCSD))
        ret = DecryptNcchNcsdFile(path, destptr, filetype, 0, 0, NULL, NULL);
    else ret = 1;
    
    if (!inplace && (ret != 0))
        f_unlink(dest); // try to get rid of the borked file
    
    return ret;
}
