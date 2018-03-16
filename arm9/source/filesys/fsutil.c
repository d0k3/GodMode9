#include "fsutil.h"
#include "fsinit.h"
#include "fsdrive.h"
#include "fsperm.h"
#include "sddata.h"
#include "vff.h"
#include "virtual.h"
#include "image.h"
#include "sha.h"
#include "sdmmc.h"
#include "ff.h"
#include "ui.h"

#define SKIP_CUR        (1UL<< 9)
#define OVERWRITE_CUR   (1UL<<10)

#define _MAX_FS_OPT     8 // max file selector options

// Volume2Partition resolution table
PARTITION VolToPart[] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
    {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}
};

uint64_t GetSDCardSize() {
    if (sdmmc_sdcard_init() != 0) return 0;
    return (u64) getMMCDevice(1)->total_size * 512;
}

bool FormatSDCard(u64 hidden_mb, u32 cluster_size, const char* label) {
    u8 mbr[0x200] = { 0 };
    u8 ncsd[0x200] = { 0 };
    u8 mbrdata[0x42] = {
        0x80, 0x01, 0x01, 0x00, 0x0C, 0xFE, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0x01, 0x01, 0x00, 0xDA, 0xFE, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x55, 0xAA
    };
    u32 sd_size = getMMCDevice(1)->total_size;
    u32 emu_sector = 1;
    u32 emu_size = (u32) ((hidden_mb * 1024 * 1024) / 512);
    u32 fat_sector = align(emu_sector + emu_size, 0x2000); // align to 4MB
    u32 fat_size = (fat_sector < sd_size) ? sd_size - fat_sector : 0;
    
    // FAT size check
    if (fat_size < 0x80000) { // minimum free space: 256MB
        ShowPrompt(false, "Error: SD card is too small");
        return false;
    }
    
    // Write protection check
    if (SD_WRITE_PROTECTED) {
        ShowPrompt(false, "SD card is write protected!\nCan't continue.");
        return false;
    }
    
    // build the MBR
    memcpy(mbrdata + 0x08, &fat_sector, 4);
    memcpy(mbrdata + 0x0C, &fat_size, 4);
    memcpy(mbrdata + 0x18, &emu_sector, 4);
    memcpy(mbrdata + 0x1C, &emu_size, 4);
    memcpy(mbr + 0x1BE, mbrdata, 0x42);
    if (hidden_mb) memcpy(mbr, "GATEWAYNAND", 12); // legacy
    else memset(mbr + 0x1CE, 0, 0x10);
    
    // one last warning....
    // 0:/Nintendo 3DS/ write permission is ignored here, this warning is enough
    if (!ShowUnlockSequence(5, "!WARNING!\n \nProceeding will format this SD.\nThis will irreversibly delete\nALL data on it."))
        return false;
    ShowString("Formatting SD, please wait..."); 
    
    // write the MBR to disk
    // !this assumes a fully deinitialized file system!
    if ((sdmmc_sdcard_init() != 0) || (sdmmc_sdcard_writesectors(0, 1, mbr) != 0) ||
        (emu_size && ((sdmmc_nand_readsectors(0, 1, ncsd) != 0) || (sdmmc_sdcard_writesectors(1, 1, ncsd) != 0)))) {
        ShowPrompt(false, "Error: SD card i/o failure");
        return false;
    }
    
    // format the SD card
    VolToPart[0].pt = 1; // workaround to prevent FatFS rebuilding the MBR
    InitSDCardFS();
    UINT c_size = cluster_size;
    
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) bkpt; // will not happen
    bool ret = ((f_mkfs("0:", FM_FAT32, c_size, buffer, STD_BUFFER_SIZE) == FR_OK) || 
        (f_mkfs("0:", FM_FAT32, 0, buffer, STD_BUFFER_SIZE) == FR_OK)) &&
        (f_setlabel((label) ? label : "0:GM9SD") == FR_OK);
    free(buffer);
    
    DeinitSDCardFS();
    VolToPart[0].pt = 0; // revert workaround to prevent SD mount problems
    
    return ret;
}

bool SetupBonusDrive(void) {
    if (!ShowUnlockSequence(3, "Format the bonus drive?\nThis will irreversibly delete\nALL data on it."))
        return false;
    ShowString("Formatting drive, please wait...");
    if (GetMountState() & IMG_NAND) InitImgFS(NULL);
    
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) bkpt;
    bool ret = (f_mkfs("8:", FM_ANY, 0, buffer, STD_BUFFER_SIZE) == FR_OK);
    free(buffer);
    
    if (ret) {
        f_setlabel("8:BONUS");
        InitExtFS();
    }
    return ret;
}

bool FileUnlock(const char* path) {
    FIL file;
    FRESULT res;
    
    if (!(DriveType(path) & DRV_FAT)) return true; // can't really check this
    if ((res = fx_open(&file, path, FA_READ | FA_OPEN_EXISTING)) != FR_OK) {
        char pathstr[32 + 1];
        TruncateString(pathstr, path, 32, 8);
        if (GetMountState() && (res == FR_LOCKED) && 
            (ShowPrompt(true, "%s\nFile is currently mounted.\nUnmount to unlock?", pathstr))) {
            InitImgFS(NULL);
            if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
                return false;
        } else return false;
    }
    fx_close(&file);
    
    return true;
}

bool FileSetData(const char* path, const void* data, size_t size, size_t foffset, bool create) {
    UINT bw;
    if (!CheckWritePermissions(path)) return false;
    if ((DriveType(path) & DRV_FAT) && create) f_unlink(path);
    return (fvx_qwrite(path, data, foffset, size, &bw) == FR_OK) && (bw == size);
}

size_t FileGetData(const char* path, void* data, size_t size, size_t foffset) {
    UINT br;
    if (fvx_qread(path, data, foffset, size, &br) != FR_OK) br = 0;
    return br;
}

size_t FileGetSize(const char* path) {
    FILINFO fno;
    if (fvx_stat(path, &fno) != FR_OK)
        return 0;
    return fno.fsize;
}

bool FileGetSha256(const char* path, u8* sha256, u64 offset, u64 size) {
    bool ret = true;
    FIL file;
    u64 fsize;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;
    
    fsize = fvx_size(&file);
    if (offset + size > fsize) return false;
    if (!size) size = fsize - offset;
    fvx_lseek(&file, offset);
    
    u32 bufsiz = min(STD_BUFFER_SIZE, fsize);
    u8* buffer = (u8*) malloc(bufsiz);
    if (!buffer) return false;
    
    ShowProgress(0, 0, path);
    sha_init(SHA256_MODE);
    for (u64 pos = 0; (pos < size) && ret; pos += bufsiz) {
        UINT read_bytes = min(bufsiz, size - pos);
        UINT bytes_read = 0;
        if (fvx_read(&file, buffer, read_bytes, &bytes_read) != FR_OK)
            ret = false;
        if (!ShowProgress(pos + bytes_read, size, path))
            ret = false;
        sha_update(buffer, bytes_read);
    }
    
    sha_get(sha256);
    fvx_close(&file);
    free(buffer);
    
    ShowProgress(1, 1, path);
    
    return ret;
}

u32 FileFindData(const char* path, u8* data, u32 size_data, u32 offset_file) {
    FIL file; // used for FAT & virtual
    u64 found = (u64) -1;
    u64 fsize = FileGetSize(path);
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return found;
    
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) return false;
    
    // main routine
    for (u32 pass = 0; pass < 2; pass++) {
        bool show_progress = false;
        u64 pos = (pass == 0) ? offset_file : 0;
        u64 search_end = (pass == 0) ? fsize : offset_file + size_data;
        search_end = (search_end > fsize) ? fsize : search_end;
        for (; (pos < search_end) && (found == (u64) -1); pos += STD_BUFFER_SIZE - (size_data - 1)) {
            UINT read_bytes = min(STD_BUFFER_SIZE, search_end - pos);
            UINT btr;
            fvx_lseek(&file, pos);
            if ((fvx_read(&file, buffer, read_bytes, &btr) != FR_OK) || (btr != read_bytes))
                break;
            for (u32 i = 0; i + size_data <= read_bytes; i++) {
                if (memcmp(buffer + i, data, size_data) == 0) {
                    found = pos + i;
                    break;
                }
            }
            if (!show_progress && (found == (u64) -1) && (pos + read_bytes < fsize)) {
                ShowProgress(0, 0, path);
                show_progress = true;
            }
            if (show_progress && (!ShowProgress(pos + read_bytes, fsize, path)))
                break;
        }
    }
    
    free(buffer);
    fvx_close(&file);
    
    return found;
}

bool FileInjectFile(const char* dest, const char* orig, u64 off_dest, u64 off_orig, u64 size, u32* flags) {
    FIL ofile;
    FIL dfile;
    bool allow_expand = (flags && (*flags & ALLOW_EXPAND));
    
    if (!CheckWritePermissions(dest)) return false;
    if (strncasecmp(dest, orig, 256) == 0) {
        ShowPrompt(false, "Error: Can't inject file into itself");
        return false;
    }
    
    // open destination / origin
    if (fvx_open(&dfile, dest, FA_WRITE | ((allow_expand) ? FA_OPEN_ALWAYS : FA_OPEN_EXISTING)) != FR_OK)
        return false;
    if ((fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) &&
        (!FileUnlock(orig) || (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))) {
        fvx_close(&dfile);
        return false;
    }
    fvx_lseek(&dfile, off_dest);
    fvx_lseek(&ofile, off_orig);
    if (!size && (off_orig < fvx_size(&ofile)))
        size = fvx_size(&ofile) - off_orig;
    
    // check file limits
    if (!allow_expand && (off_dest + size > fvx_size(&dfile))) {
        ShowPrompt(false, "Operation would write beyond end of file");
        fvx_close(&dfile);
        fvx_close(&ofile);
        return false;
    } else if (off_orig + size > fvx_size(&ofile)) {
        ShowPrompt(false, "Not enough data in file");
        fvx_close(&dfile);
        fvx_close(&ofile);
        return false;
    }
    
    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) return false;
    
    bool ret = true;
    ShowProgress(0, 0, orig);
    for (u64 pos = 0; (pos < size) && ret; pos += STD_BUFFER_SIZE) {
        UINT read_bytes = min(STD_BUFFER_SIZE, size - pos);
        UINT bytes_read = read_bytes;
        UINT bytes_written = read_bytes;
        if ((fvx_read(&ofile, buffer, read_bytes, &bytes_read) != FR_OK) ||
            (fvx_write(&dfile, buffer, read_bytes, &bytes_written) != FR_OK) ||
            (bytes_read != bytes_written))
            ret = false;
        if (ret && !ShowProgress(pos + bytes_read, size, orig)) {
            if (flags && (*flags & NO_CANCEL)) {
                ShowPrompt(false, "Cancel is not allowed here");
            } else ret = !ShowPrompt(true, "B button detected. Cancel?");
            ShowProgress(0, 0, orig);
            ShowProgress(pos + bytes_read, size, orig);
        }
    }
    ShowProgress(1, 1, orig);
    
    free(buffer);
    fvx_close(&dfile);
    fvx_close(&ofile);
    
    return ret;
}

bool FileSetByte(const char* dest, u64 offset, u64 size, u8 fillbyte, u32* flags) {
    FIL dfile;
    bool allow_expand = (flags && (*flags & ALLOW_EXPAND));
    
    if (!CheckWritePermissions(dest)) return false;
    
    // open destination
    if (fvx_open(&dfile, dest, FA_WRITE | ((allow_expand) ? FA_OPEN_ALWAYS : FA_OPEN_EXISTING)) != FR_OK)
        return false;
    fvx_lseek(&dfile, offset);
    if (!size && (offset < fvx_size(&dfile)))
        size = fvx_size(&dfile) - offset;
    
    // check file limits
    if (!allow_expand && (offset + size > fvx_size(&dfile))) {
        ShowPrompt(false, "Operation would write beyond end of file");
        fvx_close(&dfile);
        return false;
    }
    
    u32 bufsiz = min(STD_BUFFER_SIZE, size);
    u8* buffer = (u8*) malloc(bufsiz);
    if (!buffer) return false;
    memset(buffer, fillbyte, bufsiz);
    
    bool ret = true;
    ShowProgress(0, 0, dest);
    for (u64 pos = 0; (pos < size) && ret; pos += bufsiz) {
        UINT write_bytes = min(bufsiz, size - pos);
        UINT bytes_written = write_bytes;
        if ((fvx_write(&dfile, buffer, write_bytes, &bytes_written) != FR_OK) ||
            (write_bytes != bytes_written))
            ret = false;
        if (ret && !ShowProgress(pos + bytes_written, size, dest)) {
            if (flags && (*flags & NO_CANCEL)) {
                ShowPrompt(false, "Cancel is not allowed here");
            } else ret = !ShowPrompt(true, "B button detected. Cancel?");
            ShowProgress(0, 0, dest);
            ShowProgress(pos + bytes_written, size, dest);
        }
    }
    ShowProgress(1, 1, dest);
    
    free(buffer);
    fvx_close(&dfile);
    
    return ret;
}

bool FileCreateDummy(const char* cpath, const char* filename, u64 size) {
    char npath[256]; // 256 is the maximum length of a full path
    if (!CheckWritePermissions(cpath)) return false;
    if (filename) snprintf(npath, 255, "%s/%s", cpath, filename);
    else snprintf(npath, 255, "%s", cpath);
    
    // create dummy file (fail if already existing)
    // then, expand the file size via cluster preallocation
    FIL dfile;
    if (fx_open(&dfile, npath, FA_WRITE | FA_CREATE_NEW) != FR_OK)
        return false;
    f_lseek(&dfile, size > 0xFFFFFFFF ? 0xFFFFFFFF : (FSIZE_t) size);
    f_sync(&dfile);
    fx_close(&dfile);
    
    return true;
}

bool DirCreate(const char* cpath, const char* dirname) {
    char npath[256]; // 256 is the maximum length of a full path
    if (!CheckWritePermissions(cpath)) return false;
    snprintf(npath, 255, "%s/%s", cpath, dirname);
    return (fa_mkdir(npath) == FR_OK);
}

bool DirInfoWorker(char* fpath, bool virtual, u64* tsize, u32* tdirs, u32* tfiles) {
    char* fname = fpath + strnlen(fpath, 256 - 1);
    bool ret = true;
    if (virtual) {
        VirtualDir vdir;
        VirtualFile vfile;
        if (!GetVirtualDir(&vdir, fpath)) return false; // get dir reader object
        while (ReadVirtualDir(&vfile, &vdir)) {
            if (vfile.flags & VFLAG_DIR) {
                (*tdirs)++;
                *(fname++) = '/';
                GetVirtualFilename(fname, &vfile, (256 - 1) - (fname - fpath));
                if (!DirInfoWorker(fpath, virtual, tsize, tdirs, tfiles)) ret = false;
                *(--fname) = '\0';
            } else {
                *tsize += vfile.size;
                (*tfiles)++;
            }
        }
    } else {
        DIR pdir;
        FILINFO fno;
        if (fa_opendir(&pdir, fpath) != FR_OK) return false; // get dir reader object
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            if (fno.fname[0] == 0) break; // end of dir
            if (fno.fattrib & AM_DIR) {
                (*tdirs)++;
                *(fname++) = '/';
                strncpy(fname, fno.fname, (256 - 1) - (fname - fpath));
                if (!DirInfoWorker(fpath, virtual, tsize, tdirs, tfiles)) ret = false;
                *(--fname) = '\0';
            } else {
                *tsize += fno.fsize;
                (*tfiles)++;
            }
        }
        f_closedir(&pdir);
    }
    
    return ret;
}

bool DirInfo(const char* path, u64* tsize, u32* tdirs, u32* tfiles) {
    bool virtual = (DriveType(path) & DRV_VIRTUAL);
    char fpath[256];
    strncpy(fpath, path, 255);
    *tsize = *tdirs = *tfiles = 0;
    bool res = DirInfoWorker(fpath, virtual, tsize, tdirs, tfiles);
    return res;
}

bool PathExist(const char* path) {
    return (fvx_stat(path, NULL) == FR_OK);
}

bool PathMoveCopyRec(char* dest, char* orig, u32* flags, bool move, u8* buffer, u32 bufsiz) {
    bool to_virtual = GetVirtualSource(dest);
    bool silent = (flags && (*flags & SILENT));
    bool ret = false;
    
    // check destination write permission (special paths only)
    if (((*dest == '1') || (strncmp(dest, "0:/Nintendo 3DS", 16) == 0)) &&
        (!flags || !(*flags & OVERRIDE_PERM)) && 
        !CheckWritePermissions(dest)) return false;
    
    FILINFO fno;
    if (fvx_stat(orig, &fno) != FR_OK) return false; // origin does not exist
    if (move && (to_virtual || fno.fattrib & AM_VRT)) return false; // trying to move a virtual file
    
    // path string (for output)
    char deststr[36 + 1];
    TruncateString(deststr, dest, 36, 8);
    
    // the copy process takes place here
    if (!ShowProgress(0, 0, orig) && !(flags && (*flags & NO_CANCEL))) {
        if (ShowPrompt(true, "%s\nB button detected. Cancel?", deststr)) return false;
        ShowProgress(0, 0, orig);
    }
    if (move && fvx_stat(dest, NULL) != FR_OK) { // moving if dest not existing
        ret = (fvx_rename(orig, dest) == FR_OK);
    } else if (fno.fattrib & AM_DIR) { // processing folders (same for move & copy)
        DIR pdir;
        char* fname = orig + strnlen(orig, 256);
        
        // create the destination folder if it does not already exist
        if (fvx_opendir(&pdir, dest) != FR_OK) {
            if (fvx_mkdir(dest) != FR_OK) {
                if (!silent) ShowPrompt(false, "%s\nError: Overwriting file with dir", deststr);
                return false;
            }
        } else fvx_closedir(&pdir);
        
        if (fvx_opendir(&pdir, orig) != FR_OK)
            return false;
        *(fname++) = '/';
        
        while (fvx_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, 256 - (fname - orig));
            if (fno.fname[0] == 0) {
                ret = true;
                break;
            } else {
                // recursively process directory
                char* oname = strrchr(orig, '/');
                char* dname = dest + strnlen(dest, 255);
                if (oname == NULL) return false; // not a proper origin path
                strncpy(dname, oname, 256 - (dname - dest)); // copy name plus preceding '/'
                bool res = PathMoveCopyRec(dest, orig, flags, move, buffer, bufsiz);
                *dname = '\0';
                if (!res) break;
            }
        }
        
        fvx_closedir(&pdir);
        *(--fname) = '\0';
    } else if (move) { // moving if destination exists
        if (fvx_stat(dest, &fno) != FR_OK) return false;
        if (fno.fattrib & AM_DIR) {
            if (!silent) ShowPrompt(false, "%s\nError: Overwriting dir with file", deststr);
            return false;
        }
        if (fvx_unlink(dest) != FR_OK) return false;
        ret = (fvx_rename(orig, dest) == FR_OK);
    } else { // copying files
        FIL ofile;
        FIL dfile;
        u64 fsize;
        
        if (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            if (!FileUnlock(orig) || (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))
                return false;
            ShowProgress(0, 0, orig); // reinit progress bar
        }
        
        if (fvx_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            if (!silent) ShowPrompt(false, "%s\nError: Cannot open destination file", deststr);
            fvx_close(&ofile);
            return false;
        }
        
        ret = true; // destination file exists by now, so we need to handle deletion
        fsize = fvx_size(&ofile); // check space via cluster preallocation
        if ((fvx_lseek(&dfile, fsize) != FR_OK) || (fvx_sync(&dfile) != FR_OK)) {
            if (!silent) ShowPrompt(false, "%s\nError: Not enough space available", deststr);
            ret = false;
        }
        
        fvx_lseek(&dfile, 0);
        fvx_sync(&dfile);
        fvx_lseek(&ofile, 0);
        fvx_sync(&ofile);
        
        if (flags && (*flags & CALC_SHA)) sha_init(SHA256_MODE);
        for (u64 pos = 0; (pos < fsize) && ret; pos += bufsiz) {
            UINT bytes_read = 0;
            UINT bytes_written = 0;            
            if ((fvx_read(&ofile, buffer, bufsiz, &bytes_read) != FR_OK) ||
                (fvx_write(&dfile, buffer, bytes_read, &bytes_written) != FR_OK) ||
                (bytes_read != bytes_written))
                ret = false;
            if (ret && !ShowProgress(pos + bytes_read, fsize, orig)) {
                if (flags && (*flags & NO_CANCEL)) {
                    ShowPrompt(false, "%s\nCancel is not allowed here", deststr);
                } else ret = !ShowPrompt(true, "%s\nB button detected. Cancel?", deststr);
                ShowProgress(0, 0, orig);
                ShowProgress(pos + bytes_read, fsize, orig);
            }
            if (flags && (*flags & CALC_SHA))
                sha_update(buffer, bytes_read);
        }
        ShowProgress(1, 1, orig);
        
        fvx_close(&ofile);
        fvx_close(&dfile);
        if (!ret) fvx_unlink(dest);
        else if (!to_virtual && flags && (*flags & CALC_SHA)) {
            u8 sha256[0x20];
            char* ext_sha = dest + strnlen(dest, 256);
            strncpy(ext_sha, ".sha", 256 - (ext_sha - dest));
            sha_get(sha256);
            FileSetData(dest, sha256, 0x20, 0, true);
        }
    }
    
    return ret;
}

bool PathMoveCopy(const char* dest, const char* orig, u32* flags, bool move) {
    // check permissions
    if (!flags || !(*flags & OVERRIDE_PERM)) {
        if (!CheckWritePermissions(dest)) return false;
        if (move && !CheckDirWritePermissions(orig)) return false;
    }
    
    // reset local flags
    if (flags) *flags = *flags & ~(SKIP_CUR|OVERWRITE_CUR);
    
    // preparations
    int ddrvtype = DriveType(dest);
    int odrvtype = DriveType(orig);
    char ldest[256]; // 256 is the maximum length of a full path
    char lorig[256];
    strncpy(ldest, dest, 255);
    strncpy(lorig, orig, 255);
    char deststr[36 + 1];
    TruncateString(deststr, ldest, 36, 8);
    
    // moving only for regular FAT drives (= not alias drives)
    if (move && !(ddrvtype & odrvtype & DRV_STDFAT)) {
        ShowPrompt(false, "Error: Only FAT files can be moved");
        return false;
    }
    
    // is destination part of origin?
    u32 olen = strnlen(lorig, 255);
    if ((strncasecmp(ldest, lorig, olen) == 0) && (ldest[olen] == '/')) {
        ShowPrompt(false, "%s\nError: Destination is part of origin", deststr);
        return false;
    }
    
    if (!(ddrvtype & DRV_VIRTUAL)) { // FAT destination handling
        // get destination name
        char* dname = strrchr(ldest, '/');
        if (!dname) return false;
        dname++;
        
        // check & fix destination == origin
        while (strncasecmp(ldest, lorig, 255) == 0) {
            if (!ShowStringPrompt(dname, 255 - (dname - ldest), "%s\nDestination equals origin\nChoose another name?", deststr))
                return false;
        }
        
        // check if destination exists
        if (flags && !(*flags & (OVERWRITE_CUR|OVERWRITE_ALL)) && (fa_stat(ldest, NULL) == FR_OK)) {
            if (*flags & SKIP_ALL) {
                *flags |= SKIP_CUR;
                return true;
            }
            const char* optionstr[5] =
                {"Choose new name", "Overwrite file(s)", "Skip file(s)", "Overwrite all", "Skip all"};
            u32 user_select = ShowSelectPrompt((*flags & ASK_ALL) ? 5 : 3, optionstr,
                "Destination already exists:\n%s", deststr);
            if (user_select == 1) {
                do {
                    if (!ShowStringPrompt(dname, 255 - (dname - ldest), "Choose new destination name"))
                        return false;
                } while (fa_stat(ldest, NULL) == FR_OK);
            } else if (user_select == 2) {
                *flags |= OVERWRITE_CUR;
            } else if (user_select == 3) {
                *flags |= SKIP_CUR;
                return true;
            } else if (user_select == 4) {
                *flags |= OVERWRITE_ALL;
            } else if (user_select == 5) {
                *flags |= (SKIP_CUR|SKIP_ALL);
                return true;
            } else {
                return false;
            }
        }
        
        // ensure the destination path exists
        if (flags && (*flags & BUILD_PATH)) fvx_rmkpath(ldest);
        
        // setup buffer
        u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
        if (!buffer) {
            ShowPrompt(false, "Out of memory.");
            return false;
        }
        
        // actual move / copy operation
        bool same_drv = (strncasecmp(lorig, ldest, 2) == 0);
        bool res = PathMoveCopyRec(ldest, lorig, flags, move && same_drv, buffer, STD_BUFFER_SIZE);
        if (move && res && (!flags || !(*flags&SKIP_CUR))) PathDelete(lorig);
        
        free(buffer);
        return res;
    } else { // virtual destination handling
        // can't write an SHA file to a virtual destination
        if (flags) *flags |= ~CALC_SHA;
        bool force_unmount = false;
        
        // handle NAND image unmounts
        if (ddrvtype & (DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE)) {
            FILINFO fno;
            // virtual NAND files over 4 MB require unmount, totally arbitrary limit (hacky!)
            if ((fvx_stat(ldest, &fno) == FR_OK) && (fno.fsize > 4 * 1024 * 1024))
                force_unmount = true;
        }
        
        // prevent illegal operations
        if (force_unmount && (odrvtype & ddrvtype & (DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE))) {
            ShowPrompt(false, "Copy operation is not allowed");
            return false; 
        }
        
        // check destination == origin
        if (strncasecmp(ldest, lorig, 255) == 0) {
            ShowPrompt(false, "%s\nDestination equals origin", deststr);
            return false;
        }
        
        // setup buffer
        u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
        if (!buffer) {
            ShowPrompt(false, "Out of memory.");
            return false;
        }
        
        // actual virtual copy operation
        if (force_unmount) DismountDriveType(DriveType(ldest)&(DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE));
        bool res = PathMoveCopyRec(ldest, lorig, flags, false, buffer, STD_BUFFER_SIZE);
        if (force_unmount) InitExtFS();
        
        free(buffer);
        return res;
    }
}

bool PathCopy(const char* destdir, const char* orig, u32* flags) {
    // build full destination path (on top of destination directory)
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    if (oname == NULL) return false; // not a proper origin path
    snprintf(dest, 255, "%s/%s", destdir, (++oname));
    
    // virtual destination special handling
    if (GetVirtualSource(destdir)) {
        u64 osize = FileGetSize(orig);
        VirtualFile dvfile;
        if (!osize) return false;
        if (!GetVirtualFile(&dvfile, dest)) {
            VirtualDir vdir;
            if (!GetVirtualDir(&vdir, destdir)) return false;
            while (true) { // search by size should be a last resort solution
                if (!ReadVirtualDir(&dvfile, &vdir)) return false;
                if (dvfile.size == osize) break; // file found
            }
            if (!ShowPrompt(true, "Entry not found: %s\nInject into %s instead?", dest, dvfile.name))
                return false;
            snprintf(dest, 255, "%s/%s", destdir, dvfile.name);
        } else if (osize < dvfile.size) { // if origin is smaller than destination...
            char deststr[36 + 1];
            char origstr[36 + 1];
            char osizestr[32];
            char dsizestr[32];
            TruncateString(deststr, dest, 36, 8);
            TruncateString(origstr, orig, 36, 8);
            FormatBytes(osizestr, osize);
            FormatBytes(dsizestr, dvfile.size);
            if (dvfile.size > osize) {
                if (!ShowPrompt(true, "File smaller than available space:\n%s (%s)\n%s (%s)\nContinue?", origstr, osizestr, deststr, dsizestr))
                    return false;
            }
        }
    }
    
    return PathMoveCopy(dest, orig, flags, false);
}

bool PathMove(const char* destdir, const char* orig, u32* flags) {
    // build full destination path (on top of destination directory)
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    if (oname == NULL) return false; // not a proper origin path
    snprintf(dest, 255, "%s/%s", destdir, (++oname));
    
    return PathMoveCopy(dest, orig, flags, true);
}

bool PathDelete(const char* path) {
    if (!CheckDirWritePermissions(path)) return false;
    return (fvx_runlink(path) == FR_OK);
}

bool PathRename(const char* path, const char* newname) {
    char npath[256]; // 256 is the maximum length of a full path
    char* oldname = strrchr(path, '/');
    
    if (!CheckDirWritePermissions(path)) return false;
    if (!oldname) return false;
    oldname++;
    strncpy(npath, path, oldname - path);
    strncpy(npath + (oldname - path), newname, 255 - (oldname - path));
    
    return (f_rename(path, npath) == FR_OK);
}

bool PathAttr(const char* path, u8 attr, u8 mask) {
    if (!CheckDirWritePermissions(path)) return false;
    return (f_chmod(path, attr, mask) == FR_OK);
}

bool FileSelectorWorker(char* result, const char* text, const char* path, const char* pattern, u32 flags, void* buffer) {
    DirStruct* contents = (DirStruct*) buffer;
    char path_local[256];
    strncpy(path_local, path, 256);
    
    bool no_dirs = flags & NO_DIRS;
    bool no_files = flags & NO_FILES;
    bool hide_ext = flags & HIDE_EXT;
    bool select_dirs = flags & SELECT_DIRS;
    
    // main loop
    while (true) {
        u32 n_found = 0;
        u32 pos = 0;
        GetDirContents(contents, path_local);
        
        while (pos < contents->n_entries) {
            char opt_names[_MAX_FS_OPT+1][32+1];
            DirEntry* res_entry[_MAX_FS_OPT+1] = { NULL };
            u32 n_opt = 0;
            for (; pos < contents->n_entries; pos++) {
                DirEntry* entry = &(contents->entry[pos]);
                if (((entry->type == T_DIR) && no_dirs) ||
                    ((entry->type == T_FILE) && (no_files || (fvx_match_name(entry->name, pattern) != FR_OK))) ||
                    (entry->type == T_DOTDOT) || (strncmp(entry->name, "._", 2) == 0))
                    continue;
                if (n_opt == _MAX_FS_OPT) {
                    snprintf(opt_names[n_opt++], 32, "[more...]");
                    break;
                }
                
                char temp_str[256];
                snprintf(temp_str, 256, entry->name);
                if (hide_ext && (entry->type == T_FILE)) {
                    char* dot = strrchr(temp_str, '.');
                    if (dot) *dot = '\0';
                }
                TruncateString(opt_names[n_opt], temp_str, 32, 8);
                res_entry[n_opt++] = entry;
                n_found++;
            }
            if ((pos >= contents->n_entries) && (n_opt < n_found))
                snprintf(opt_names[n_opt++], 32, "[more...]");
            if (!n_opt) break;
            
            const char* optionstr[_MAX_FS_OPT+1] = { NULL };
            for (u32 i = 0; i <= _MAX_FS_OPT; i++) optionstr[i] = opt_names[i];
            u32 user_select = ShowSelectPrompt(n_opt, optionstr, text);
            if (!user_select) return false;
            DirEntry* res_local = res_entry[user_select-1];
            if (res_local && (res_local->type == T_DIR)) { // selected dir
                if (select_dirs) {
                    strncpy(result, res_local->path, 256);
                    return true;
                } else if (FileSelectorWorker(result, text, res_local->path, pattern, flags, buffer)) {
                    return true;
                }
                break;
            } else if (res_local && (res_local->type == T_FILE)) { // selected file
                strncpy(result, res_local->path, 256);
                return true;
            }
        }
        if (!n_found) { // not a single matching entry found
            char pathstr[32+1];
            TruncateString(pathstr, path_local, 32, 8);
            ShowPrompt(false, "%s\nNo usable entries found.", pathstr);
            return false; 
        }
    }
}

bool FileSelector(char* result, const char* text, const char* path, const char* pattern, u32 flags) {
    void* buffer = (void*) malloc(sizeof(DirStruct));
    if (!buffer) return false;
    
    bool ret = FileSelectorWorker(result, text, path, pattern, flags, buffer);
    free(buffer);
    return ret;
}
