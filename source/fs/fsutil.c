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

#define SKIP_CUR (1UL<<3)
#define OVERWRITE_CUR (1UL<<4)

// Volume2Partition resolution table
PARTITION VolToPart[] = {
    {0, 1}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
    {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}
};

uint64_t GetSDCardSize() {
    if (sdmmc_sdcard_init() != 0) return 0;
    return (u64) getMMCDevice(1)->total_size * 512;
}

bool FormatSDCard(u64 hidden_mb, u32 cluster_size, const char* label) {
    u8 mbr[0x200] = { 0 };
    u8 mbrdata[0x42] = {
        0x80, 0x01, 0x01, 0x00, 0x0C, 0xFE, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0x01, 0x01, 0x00, 0x1C, 0xFE, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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
    sd_size = fat_size;
    
    // build the MBR
    memcpy(mbrdata + 0x08, &fat_sector, 4);
    memcpy(mbrdata + 0x0C, &fat_size, 4);
    memcpy(mbrdata + 0x18, &emu_sector, 4);
    memcpy(mbrdata + 0x1C, &emu_size, 4);
    memcpy(mbr + 0x1BE, mbrdata, 0x42);
    if (hidden_mb) memcpy(mbr, "GATEWAYNAND", 12);
    else memset(mbr + 0x1CE, 0, 0x10);
    
    // one last warning....
    if (!ShowUnlockSequence(6, "!WARNING!\n \nProceeding will format this SD.\nThis will irreversibly delete\nALL data on it.\n"))
        return false;
    ShowString("Formatting SD, please wait..."); 
    
    // write the MBR to disk
    // !this assumes a fully deinitialized file system!
    if ((sdmmc_sdcard_init() != 0) || (sdmmc_sdcard_writesectors(0, 1, mbr) != 0)) {
        ShowPrompt(false, "Error: SD card i/o failure");
        return false;
    }
    
    // format the SD card
    InitSDCardFS();
    UINT c_size = cluster_size;
    bool ret = (f_mkfs("0:", FM_FAT32, c_size, MAIN_BUFFER, MAIN_BUFFER_SIZE) == FR_OK) && (f_setlabel((label) ? label : "0:GM9SD") == FR_OK);
    DeinitSDCardFS();
    
    return ret;
}

bool SetupBonusDrive(void) {
    if (!ShowUnlockSequence(3, "Format the bonus drive?\nThis will irreversibly delete\nALL data on it.\n"))
        return false;
    ShowString("Formatting drive, please wait...");
    if (GetMountState() & IMG_NAND) InitImgFS(NULL);
    bool ret = (f_mkfs("8:", FM_ANY, 0, MAIN_BUFFER, MAIN_BUFFER_SIZE) == FR_OK);
    if (ret) {
        f_setlabel("8:BONUS");
        InitExtFS();
    }
    return ret;
}

bool FileUnlock(const char* path) {
    FIL file;
    if (!(DriveType(path) & DRV_FAT)) return true; // can't really check this
    if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        char pathstr[32 + 1];
        TruncateString(pathstr, path, 32, 8);
        if (GetMountState() && (strncmp(path, GetMountPath(), 256) == 0) && 
            (ShowPrompt(true, "%s\nFile is currently mounted.\nUnmount to unlock?", pathstr))) {
            InitImgFS(NULL);
            if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
                return false;
        } else return false;
    }
    fx_close(&file);
    return true;
}

bool FileSetData(const char* path, const u8* data, size_t size, size_t foffset, bool create) {
    UINT bw;
    if (!CheckWritePermissions(path)) return false;
    if ((DriveType(path) & DRV_FAT) && create) f_unlink(path);
    return (fvx_qwrite(path, data, foffset, size, &bw) == FR_OK) && (bw == size);
}

size_t FileGetData(const char* path, u8* data, size_t size, size_t foffset) {
    UINT br;
    if (fvx_qread(path, data, foffset, size, &br) != FR_OK) br = 0;
    return br;
}

size_t FileGetSize(const char* path) {
    int drvtype = DriveType(path);
    if (drvtype & DRV_FAT) {
        FILINFO fno;
        if (fa_stat(path, &fno) != FR_OK)
            return 0;
        return fno.fsize;
    } else if (drvtype & DRV_VIRTUAL) {
        VirtualFile vfile;
        if (!GetVirtualFile(&vfile, path))
            return 0;
        return vfile.size;
    }
    return 0;
}

bool FileGetSha256(const char* path, u8* sha256) {
    bool ret = true;
    FIL file;
    u64 fsize;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;
    fsize = fvx_size(&file);
    fvx_lseek(&file, 0);
    ShowProgress(0, 0, path);
    sha_init(SHA256_MODE);
    for (u64 pos = 0; (pos < fsize) && ret; pos += MAIN_BUFFER_SIZE) {
        UINT bytes_read = 0;
        if (fvx_read(&file, MAIN_BUFFER, MAIN_BUFFER_SIZE, &bytes_read) != FR_OK)
            ret = false;
        if (!ShowProgress(pos + bytes_read, fsize, path))
            ret = false;
        sha_update(MAIN_BUFFER, bytes_read);
    }
    sha_get(sha256);
    fvx_close(&file);
    ShowProgress(1, 1, path);
    
    return ret;
}

u32 FileFindData(const char* path, u8* data, u32 size_data, u32 offset_file) {
    FIL file; // used for FAT & virtual
    u64 found = (u64) -1;
    u64 fsize = FileGetSize(path);
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return found;
    
    // main routine
    for (u32 pass = 0; pass < 2; pass++) {
        bool show_progress = false;
        u64 pos = (pass == 0) ? offset_file : 0;
        u64 search_end = (pass == 0) ? fsize : offset_file + size_data;
        search_end = (search_end > fsize) ? fsize : search_end;
        for (; (pos < search_end) && (found == (u64) -1); pos += MAIN_BUFFER_SIZE - (size_data - 1)) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, search_end - pos);
            UINT btr;
            fvx_lseek(&file, pos);
            if ((fvx_read(&file, MAIN_BUFFER, read_bytes, &btr) != FR_OK) || (btr != read_bytes))
                break;
            for (u32 i = 0; i + size_data <= read_bytes; i++) {
                if (memcmp(MAIN_BUFFER + i, data, size_data) == 0) {
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
    fvx_close(&file);
    
    return found;
}

bool FileInjectFile(const char* dest, const char* orig, u32 offset) {
    FIL ofile;
    FIL dfile;
    u64 osize;
    u64 dsize;
    bool ret;
    
    if (!CheckWritePermissions(dest)) return false;
    if (strncmp(dest, orig, 256) == 0) {
        ShowPrompt(false, "Error: Can't inject file into itself");
        return false;
    }
    
    // open destination / origin
    if (fvx_open(&dfile, dest, FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return false;
    if ((fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) &&
        (!FileUnlock(orig) || (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))) {
        fvx_close(&dfile);
        return false;
    }
    fvx_lseek(&dfile, offset);
    fvx_lseek(&ofile, 0);
    dsize = fvx_size(&dfile);
    osize = f_size(&ofile);
    
    // check file limits
    if (offset + osize > dsize) {
        ShowPrompt(false, "Operation would write beyond end of file");
        fvx_close(&dfile);
        fvx_close(&ofile);
        return false;
    }
    
    ret = true;
    ShowProgress(0, 0, orig);
    for (u64 pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
        UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
        UINT bytes_read = read_bytes;
        UINT bytes_written = read_bytes;
        if (fvx_read(&ofile, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK) {
            ret = false;
            break;
        }
        if ((!ShowProgress(pos + (bytes_read / 2), osize, orig)) ||
            (fvx_write(&dfile, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK) ||
            (bytes_read != bytes_written))
            ret = false;
    }
    ShowProgress(1, 1, orig);
    
    fvx_close(&dfile);
    fvx_close(&ofile);
    
    return ret;
}

bool PathCopyVrtToVrt(const char* destdir, const char* orig) {
    VirtualFile dvfile;
    VirtualFile ovfile;
    bool ret = true;
    
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    if (oname == NULL) return false; // not a proper origin path
    snprintf(dest, 255, "%s/%s", destdir, (++oname));
    
    char deststr[36 + 1];
    char origstr[36 + 1];
    TruncateString(deststr, dest, 36, 8);
    TruncateString(origstr, orig, 36, 8);
    
    if (!GetVirtualFile(&dvfile, dest) || !GetVirtualFile(&ovfile, orig))
        return false;
    u64 osize = ovfile.size;
    if (dvfile.size != osize) { // almost impossible, but so what...
        ShowPrompt(false, "Virtual file size mismatch:\n%s\n%s", origstr, deststr);
        return false;
    } else if (strncmp(dest, orig, 256) == 0) { // destination == origin
        ShowPrompt(false, "Origin equals destination:\n%s\n%s", origstr, deststr);
        return false;
    }
    if ((dvfile.keyslot == ovfile.keyslot) && (dvfile.offset == ovfile.offset))
        dvfile.keyslot = ovfile.keyslot = 0xFF; // this improves copy times for virtual NAND
    
    // check write permissions
    if (!CheckWritePermissions(dest)) return false;
    
    // unmount critical NAND drives
    DismountDriveType(DriveType(destdir)&(DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE));
    if (!ShowProgress(0, 0, orig)) ret = false;
    for (u64 pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
        UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
        if (ReadVirtualFile(&ovfile, MAIN_BUFFER, pos, read_bytes, NULL) != 0)
            ret = false;
        if (!ShowProgress(pos + (read_bytes / 2), osize, orig))
            ret = false;
        if (WriteVirtualFile(&dvfile, MAIN_BUFFER, pos, read_bytes, NULL) != 0)
            ret = false;
    }
    ShowProgress(1, 1, orig);
    InitExtFS();
    
    return ret;
}

bool PathCopyFatToVrt(const char* destdir, const char* orig) {
    VirtualFile dvfile;
    FIL ofile;
    bool ret = true;
    
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    if (oname == NULL) return false; // not a proper origin path
    snprintf(dest, 255, "%s/%s", destdir, (++oname));
    
    // FAT file size
    FILINFO fno;
    if (fa_stat(orig, &fno) != FR_OK) return false; // file does not exist
    u64 osize = fno.fsize;
    
    // virtual file
    if (!GetVirtualFile(&dvfile, dest)) {
        VirtualDir vdir;
        if (!GetVirtualDir(&vdir, destdir)) return false;
        while (true) { // search by size should be a last resort solution
            if (!ReadVirtualDir(&dvfile, &vdir))
                return false;
            if (dvfile.size == osize) 
                break; // file found
        }
        if (!ShowPrompt(true, "Entry not found: %s\nInject into %s instead?", dest, dvfile.name))
            return false;
        snprintf(dest, 255, "%s/%s", destdir, dvfile.name);
    } else if (dvfile.size != osize) { // handling for differing sizes
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
        } else {
            ShowPrompt(false, "File bigger than available space:\n%s (%s)\n%s (%s)", origstr, osizestr, deststr, dsizestr);
                return false;
        }
    }
    
    // check write permissions
    if (!CheckWritePermissions(dest)) return false;
    
    // FAT file
    if ((fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) &&
        (!FileUnlock(orig) || (fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))) {
        return false;
    }
    f_lseek(&ofile, 0);
    f_sync(&ofile);
    
    // unmount critical NAND drives
    DismountDriveType(DriveType(destdir)&(DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE));
    if (!ShowProgress(0, 0, orig)) ret = false;
    for (u64 pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
        UINT bytes_read = 0;           
        if (fx_read(&ofile, MAIN_BUFFER, MAIN_BUFFER_SIZE, &bytes_read) != FR_OK)
            ret = false;
        if (!ShowProgress(pos + (bytes_read / 2), osize, orig))
            ret = false;
        if (WriteVirtualFile(&dvfile, MAIN_BUFFER, pos, bytes_read, NULL) != 0)
            ret = false;
    }
    ShowProgress(1, 1, orig);
    fx_close(&ofile);
    InitExtFS();
    
    return ret;
}

bool PathCopyVrtToFat(char* dest, char* orig, u32* flags) {
    VirtualFile vfile;
    FILINFO fno;
    bool ret = false;
    
    if (fa_stat(dest, &fno) != FR_OK) { // is root or destination does not exist
        DIR tmp_dir; // check if root
        if (fa_opendir(&tmp_dir, dest) != FR_OK) return false;
        f_closedir(&tmp_dir);
    } else if (!(fno.fattrib & AM_DIR)) return false; // destination is not a directory (must be at this point)
    
    // build full destination path (on top of destination directory)
    char* oname = strrchr(orig, '/');
    char* dname = dest + strnlen(dest, 255);
    if (oname == NULL) return false; // not a proper origin path
    oname++;
    *(dname++) = '/';
    strncpy(dname, oname, 256 - (dname - dest));
    
    // open / check virtual file
    if (!GetVirtualFile(&vfile, orig))
        return false;
    
    // check if destination exists
    if (flags && !(*flags & (OVERWRITE_CUR|OVERWRITE_ALL)) && (fa_stat(dest, NULL) == FR_OK)) {
        if (*flags & SKIP_ALL) {
            *flags |= SKIP_CUR;
            return true;
        }
        const char* optionstr[5] =
            {"Choose new name", "Overwrite file(s)", "Skip file(s)", "Overwrite all", "Skip all"};
        char namestr[36 + 1];
        TruncateString(namestr, dest, 36, 8);
        u32 user_select = ShowSelectPrompt((*flags & ASK_ALL) ? 5 : 3, optionstr,
            "Destination already exists:\n%s", namestr);
        if (user_select == 1) {
            do {
                if (!ShowStringPrompt(dname, 255 - (dname - dest), "Choose new destination name"))
                    return false;
            } while (fa_stat(dest, NULL) == FR_OK);
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
    
    // check destination write permission (SysNAND CTRNAND only)
    if ((*dest == '1') && (!flags || (*flags & (OVERWRITE_CUR|OVERWRITE_ALL))) &&
        !CheckWritePermissions(dest)) return false;
    
    // the copy process takes place here
    if (!ShowProgress(0, 0, orig)) return false;
    if (vfile.flags & VFLAG_DIR) { // processing folders
        DIR pdir;
        VirtualDir vdir;
        char* fname = orig + strnlen(orig, 256);
        
        // create the destination folder if it does not already exist
        if (fa_opendir(&pdir, dest) != FR_OK) {
            if (f_mkdir(dest) != FR_OK) {
                ShowPrompt(false, "Error: Overwriting file with dir");
                return false;
            }
        } else f_closedir(&pdir);
        
        if (!OpenVirtualDir(&vdir, &vfile))
            return false;
        *(fname++) = '/';
        while (true) {
            if (!ReadVirtualDir(&vfile, &vdir)) {
                ret = true;
                break;
            }
            char name[256];
            if (!GetVirtualFilename(name, &vfile, 256)) break;
            strncpy(fname, name, 256 - (fname - orig));
            if (!PathCopyVrtToFat(dest, orig, flags))
                break;
        }
    } else { // copying files
        FIL dfile;
        u64 osize = vfile.size;
    
        if (GetFreeSpace(dest) < osize) {
            ShowPrompt(false, "Error: File is too big for destination");
            return false;
        }
        
        if (fx_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            ShowPrompt(false, "Error: Cannot create destination file");
            return false;
        }
        f_lseek(&dfile, 0);
        f_sync(&dfile);
    
        ret = true;
        for (u64 pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
            UINT bytes_written = 0;
            if (ReadVirtualFile(&vfile, MAIN_BUFFER, pos, read_bytes, NULL) != 0)
                ret = false;
            if (!ShowProgress(pos + (read_bytes / 2), osize, orig))
                ret = false;
            if (fx_write(&dfile, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK)
                ret = false;
            if (read_bytes != bytes_written)
                ret = false;
        }
        ShowProgress(1, 1, orig);
        
        fx_close(&dfile);
        if (!ret) f_unlink(dest);
    }
    
    *(--dname) = '\0';
    return ret;
}

bool PathCopyWorker(char* dest, char* orig, u32* flags, bool move) {
    FILINFO fno;
    bool ret = false;
      
    if (fa_stat(dest, &fno) != FR_OK) { // is root or destination does not exist
        DIR tmp_dir; // check if root
        if (fa_opendir(&tmp_dir, dest) != FR_OK) return false;
        f_closedir(&tmp_dir);
    } else if (!(fno.fattrib & AM_DIR)) return false; // destination is not a directory (must be at this point)
    if (fa_stat(orig, &fno) != FR_OK) return false; // origin does not exist

    // build full destination path (on top of destination directory)
    char* oname = strrchr(orig, '/');
    char* dname = dest + strnlen(dest, 255);
    if (oname == NULL) return false; // not a proper origin path
    oname++;
    *(dname++) = '/';
    strncpy(dname, oname, 256 - (dname - dest));
    
    // check if destination is part of or equal origin
    while (strncmp(dest, orig, 255) == 0) {
        if (!ShowStringPrompt(dname, 255 - (dname - dest), "Destination is equal to origin\nChoose another name?"))
            return false;
    }
    if (strncmp(dest, orig, strnlen(orig, 255)) == 0) {
        if ((dest[strnlen(orig, 255)] == '/') || (dest[strnlen(orig, 255)] == '\0')) {
            ShowPrompt(false, "Error: Destination is part of origin");
            return false;
        }
    }
    
    // check if destination exists
    if (flags && !(*flags & (OVERWRITE_CUR|OVERWRITE_ALL)) && (fa_stat(dest, NULL) == FR_OK)) {
        if (*flags & SKIP_ALL) {
            *flags |= SKIP_CUR;
            return true;
        }
        const char* optionstr[5] =
            {"Choose new name", "Overwrite file(s)", "Skip file(s)", "Overwrite all", "Skip all"};
        char namestr[36 + 1];
        TruncateString(namestr, dest, 36, 8);
        u32 user_select = ShowSelectPrompt((*flags & ASK_ALL) ? 5 : 3, optionstr,
            "Destination already exists:\n%s", namestr);
        if (user_select == 1) {
            do {
                if (!ShowStringPrompt(dname, 255 - (dname - dest), "Choose new destination name"))
                    return false;
            } while (fa_stat(dest, NULL) == FR_OK);
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
    
    // check destination write permission (SysNAND CTRNAND only)
    if ((*dest == '1') && (!flags || (*flags & (OVERWRITE_CUR|OVERWRITE_ALL))) &&
        !CheckWritePermissions(dest)) return false;
    
    // the copy process takes place here
    if (!ShowProgress(0, 0, orig)) return false;
    if (move && fa_stat(dest, NULL) != FR_OK) { // moving if dest not existing
        ret = (f_rename(orig, dest) == FR_OK);
    } else if (fno.fattrib & AM_DIR) { // processing folders (same for move & copy)
        DIR pdir;
        char* fname = orig + strnlen(orig, 256);
        
        // create the destination folder if it does not already exist
        if (fa_opendir(&pdir, dest) != FR_OK) {
            if (f_mkdir(dest) != FR_OK) {
                ShowPrompt(false, "Error: Overwriting file with dir");
                return false;
            }
        } else f_closedir(&pdir);
        
        if (fa_opendir(&pdir, orig) != FR_OK)
            return false;
        *(fname++) = '/';
        
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, 256 - (fname - orig));
            if (fno.fname[0] == 0) {
                ret = true;
                break;
            } else if (!PathCopyWorker(dest, orig, flags, move)) {
                break;
            }
        }
        f_closedir(&pdir);
    } else if (move) { // moving if destination exists
        if (fa_stat(dest, &fno) != FR_OK)
            return false;
        if (fno.fattrib & AM_DIR) {
            ShowPrompt(false, "Error: Overwriting dir with file");
            return false;
        }
        if (f_unlink(dest) != FR_OK)
            return false;
        ret = (f_rename(orig, dest) == FR_OK);
    } else { // copying files
        FIL ofile;
        FIL dfile;
        u64 fsize;
        
        if (fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            if (!FileUnlock(orig) || (fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))
                return false;
            ShowProgress(0, 0, orig); // reinit progress bar
        }
        
        fsize = f_size(&ofile);
        if (GetFreeSpace(dest) < fsize) {
            ShowPrompt(false, "Error: File is too big for destination");
            fx_close(&ofile);
            return false;
        }
        
        if (fx_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            ShowPrompt(false, "Error: Cannot create destination file");
            fx_close(&ofile);
            return false;
        }
        
        f_lseek(&dfile, 0);
        f_sync(&dfile);
        f_lseek(&ofile, 0);
        f_sync(&ofile);
        
        ret = true;
        for (u64 pos = 0; (pos < fsize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT bytes_read = 0;
            UINT bytes_written = 0;            
            if (fx_read(&ofile, MAIN_BUFFER, MAIN_BUFFER_SIZE, &bytes_read) != FR_OK)
                ret = false;
            if (!ShowProgress(pos + (bytes_read / 2), fsize, orig))
                ret = false;
            if (fx_write(&dfile, MAIN_BUFFER, bytes_read, &bytes_written) != FR_OK)
                ret = false;
            if (bytes_read != bytes_written)
                ret = false;
        }
        ShowProgress(1, 1, orig);
        
        fx_close(&ofile);
        fx_close(&dfile);
        if (!ret) f_unlink(dest);
    }
    
    *(--dname) = '\0';
    return ret;
}

bool PathCopy(const char* destdir, const char* orig, u32* flags) {
    if (!CheckWritePermissions(destdir)) return false;
    if (flags) *flags = *flags & ~(SKIP_CUR|OVERWRITE_CUR); // reset local flags
    int ddrvtype = DriveType(destdir);
    int odrvtype = DriveType(orig);
    if (!(ddrvtype & DRV_VIRTUAL)) { // FAT / virtual to FAT
        char fdpath[256]; // 256 is the maximum length of a full path
        char fopath[256];
        strncpy(fdpath, destdir, 255);
        strncpy(fopath, orig, 255);
        bool res = (odrvtype & DRV_VIRTUAL) ? PathCopyVrtToFat(fdpath, fopath, flags) :
            PathCopyWorker(fdpath, fopath, flags, false);
        return res;
    } else if (!(odrvtype & DRV_VIRTUAL)) { // FAT to virtual
        if (odrvtype & ddrvtype & (DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE)) {
            ShowPrompt(false, "Copy operation is not allowed");
            return false; // prevent illegal operations
        }
        return PathCopyFatToVrt(destdir, orig);
    } else return PathCopyVrtToVrt(destdir, orig); // virtual to virtual
}

bool PathMove(const char* destdir, const char* orig, u32* flags) {
    if (!CheckWritePermissions(destdir)) return false;
    if (!CheckDirWritePermissions(orig)) return false; // check orig FULL DIR permissions
    if (flags) *flags = *flags & ~(SKIP_CUR|OVERWRITE_CUR); // reset local flags
    // moving only for regular FAT drives (= not alias drives)
    if (!(DriveType(destdir) & DriveType(orig) & DRV_STDFAT)) {
        ShowPrompt(false, "Error: Moving is not possible here");
        return false;
    } else {
        char fdpath[256]; // 256 is the maximum length of a full path
        char fopath[256];
        strncpy(fdpath, destdir, 255);
        strncpy(fopath, orig, 255);
        bool same_drv = (strncmp(orig, destdir, 2) == 0);
        bool res = PathCopyWorker(fdpath, fopath, flags, same_drv);
        if (res && (!flags || !(*flags&SKIP_CUR))) PathDelete(orig);
        return res;
    }
}

bool PathDeleteWorker(char* fpath) {
    FILINFO fno;
    
    // this code handles directory content deletion
    if (fa_stat(fpath, &fno) != FR_OK) return false; // fpath does not exist
    if (fno.fattrib & AM_DIR) { // process folder contents
        DIR pdir;
        char* fname = fpath + strnlen(fpath, 255);
        
        if (fa_opendir(&pdir, fpath) != FR_OK)
            return false;
        *(fname++) = '/';
        
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, fpath + 255 - fname);
            if (fno.fname[0] == 0) {
                break;
            } else { // return value won't matter
                PathDeleteWorker(fpath);
            }
        }
        f_closedir(&pdir);
        *(--fname) = '\0';
    }
    
    return (f_unlink(fpath) == FR_OK);
}

bool PathDelete(const char* path) {
    char fpath[256]; // 256 is the maximum length of a full path
    if (!CheckDirWritePermissions(path)) return false;
    strncpy(fpath, path, 256);
    ShowString("Deleting files, please wait...");
    return PathDeleteWorker(fpath);
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

bool DirCreate(const char* cpath, const char* dirname) {
    char npath[256]; // 256 is the maximum length of a full path
    if (!CheckWritePermissions(cpath)) return false;
    snprintf(npath, 255, "%s/%s", cpath, dirname);
    return (f_mkdir(npath) == FR_OK);
}

void CreateScreenshot() {
    const u8 bmp_header[54] = {
        0x42, 0x4D, 0x36, 0xCA, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x90, 0x01, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xCA, 0x08, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    u8* buffer = MAIN_BUFFER + 54;
    u8* buffer_t = buffer + (400 * 240 * 3);
    char filename[16];
    static u32 n = 0;
    
    for (; n < 1000; n++) {
        snprintf(filename, 16, "0:/snap%03i.bmp", (int) n);
        if (fa_stat(filename, NULL) != FR_OK) break;
    }
    if (n >= 1000) return;
    
    memcpy(MAIN_BUFFER, bmp_header, 54);
    memset(buffer, 0x1F, 400 * 240 * 3 * 2);
    for (u32 x = 0; x < 400; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer_t + (y*400 + x) * 3, TOP_SCREEN + (x*240 + y) * 3, 3);
    for (u32 x = 0; x < 320; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer + (y*400 + x + 40) * 3, BOT_SCREEN + (x*240 + y) * 3, 3);
    FileSetData(filename, MAIN_BUFFER, 54 + (400 * 240 * 3 * 2), 0, true);
}
