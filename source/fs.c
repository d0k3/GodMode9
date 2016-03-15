#include "draw.h"
#include "fs.h"
#include "fatfs/ff.h"

#define MAX_FS  7


// don't use this area for anything else!
static FATFS* fs = (FATFS*)0x20316000; 

// this is the main buffer
static u8* main_buffer = (u8*)0x21200000;
// this is the main buffer size
static size_t main_buffer_size = 1 * 1024 * 1024;

// write permission level - careful with this
static u32 write_permission_level = 1;

// number of currently open file systems
static bool fs_mounted[MAX_FS] = { false };

bool InitSDCardFS() {
    #ifndef EXEC_GATEWAY
    // TODO: Magic?
    *(u32*)0x10000020 = 0;
    *(u32*)0x10000020 = 0x340;
    #endif
    fs_mounted[0] = (f_mount(fs, "0:", 1) == FR_OK);
    return fs_mounted[0];
}

bool InitNandFS() {
    for (u32 i = 1; i < MAX_FS; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (f_mount(fs + i, fsname, 1) != FR_OK) return false;
        fs_mounted[i] = true;
    }
    return true;
}

void DeinitFS() {
    for (u32 i = 0; i < MAX_FS; i++) {
        if (fs_mounted[i]) {
            char fsname[8];
            snprintf(fsname, 7, "%lu:", i);
            f_mount(NULL, fsname, 1);
            fs_mounted[i] = false;
        }
    }
}

bool CheckWritePermissions(const char* path) {
    u32 pdrv = (*path) - '0';
    
    if ((pdrv > 6) || (*(path+1) != ':')) {
        ShowPrompt(false, "Invalid path");
        return false;
    }
        
    if ((pdrv >= 1) && (pdrv <= 3) && (write_permission_level < 3)) {
        if (ShowPrompt(true, "Writing to the SysNAND is locked!\nUnlock it now?"))
            return SetWritePermissions(3);
        return false;
    } else if ((pdrv >= 4) && (pdrv <= 6) && (write_permission_level < 2)) {
        if (ShowPrompt(true, "Writing to the EmuNAND is locked!\nUnlock it now?"))
            return SetWritePermissions(2);
        return false;
    } else if ((pdrv == 0) && (write_permission_level < 1)) {
        if (ShowPrompt(true, "Writing to the SD card is locked!\nUnlock it now?"))
            return SetWritePermissions(1);
        return false;
    }
        
    return true;
}

bool SetWritePermissions(u32 level) {
    if (write_permission_level >= level) {
        // no need to ask the user here
        write_permission_level = level;
        return true;
    }
    
    switch (level) {
        case 1:
            if (!ShowUnlockSequence(1, "You want to enable SD card\nwriting permissions."))
                return false;
            break;
        case 2:
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND\nwriting permissions.\nThis is potentially dangerous!\nKeep a backup, just in case."))
                return false;
            break;
        case 3:
            if (!ShowUnlockSequence(3, "!This is your only warning!\n \nYou want to enable SysNAND\nwriting permissions.\nThis is potentially dangerous\nand can brick your 3DS!\nHaving a SysNAND backup and\nNANDmod is recommended."))
                return false;
            break;
        default:
            break;
    }
    
    write_permission_level = level;
    
    return true;
}

u32 GetWritePermissions() {
    return write_permission_level;
}

bool FileCreateData(const char* path, u8* data, size_t size) {
    FIL file;
    UINT bytes_written = 0;
    if (!CheckWritePermissions(path)) return false;
    if (f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;
    f_write(&file, data, size, &bytes_written);
    f_close(&file);
    return (bytes_written == size);
}

bool FileGetData(const char* path, u8* data, size_t size, size_t foffset)
{
    FIL file;
    UINT bytes_read = 0;
    if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;
    f_lseek(&file, foffset);
    f_read(&file, data, size, &bytes_read);
    f_close(&file);
    return (bytes_read == size);
}

bool PathCopyWorker(char* dest, char* orig, bool overwrite) {
    FILINFO fno = {.lfname = NULL};
    bool ret = false;
    
    
    if (f_stat(dest, &fno) != FR_OK) { // is root or destination does not exist
        DIR tmp_dir; // check if root
        if (f_opendir(&tmp_dir, dest) != FR_OK) return false;
        f_closedir(&tmp_dir);
    } else if (!(fno.fattrib & AM_DIR)) return false; // destination is not a directory (must be at this point)
    if (f_stat(orig, &fno) != FR_OK) return false; // origin does not exist

    // build full destination path (on top of destination directory)
    char* oname = strrchr(orig, '/');
    char* dname = dest + strnlen(dest, 255);
    if (oname == NULL) return false; // not a proper origin path
    oname++;
    *(dname++) = '/';
    strncpy(dname, oname, 256 - (dname - dest));
    
    // check if destination is part of or equal origin
    if (strncmp(dest, orig, strnlen(orig, 255)) == 0) {
        if ((dest[strnlen(orig, 255)] == '/') || (dest[strnlen(orig, 255)] == '\0')) {
            ShowPrompt(false, "Error: Destination is part of origin");
            return false;
        }
    }
    
    // check if destination exists
    if (!overwrite && (f_stat(dest, NULL) == FR_OK)) {
        char namestr[36 + 1];
        TruncateString(namestr, dest, 36, 8);
        if (!ShowPrompt(true, "Destination already exists:\n%s\nOverwrite existing file(s)?", namestr))
            return false;
        overwrite = true;
    }
    
    // the copy process takes place here
    if (!ShowProgress(0, 0, orig)) return false;
    if (fno.fattrib & AM_DIR) { // processing folders...
        DIR pdir;
        char* fname = orig + strnlen(orig, 256);
        
        // create the destination folder if it does not already exist
        if ((f_stat(dest, NULL) != FR_OK) && (f_mkdir(dest) != FR_OK))
            return false;
        
        if (f_opendir(&pdir, orig) != FR_OK)
            return false;
        *(fname++) = '/';
        fno.lfname = fname;
        fno.lfsize = 256 - (fname - orig);
        
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            if (fname[0] == 0)
                strncpy(fname, fno.fname, 256 - (fname - orig));
            if (fno.fname[0] == 0) {
                ret = true;
                break;
            } else if (!PathCopyWorker(dest, orig, overwrite)) {
                break;
            }
        }
        f_closedir(&pdir);
    } else { // processing files...
        FIL ofile;
        FIL dfile;
        size_t fsize;
        
        if (f_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return false;
        fsize = f_size(&ofile);
        if (GetFreeSpace(dest) < fsize) {
            ShowPrompt(false, "Error: File is too big for destination");
            f_close(&ofile);
            return false;
        }
        
        if (f_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            f_close(&ofile);
            return false;
        }
        
        f_lseek(&dfile, 0);
        f_sync(&dfile);
        f_lseek(&ofile, 0);
        f_sync(&ofile);
        
        ret = true;
        for (size_t pos = 0; (pos < fsize) && ret; pos += main_buffer_size) {
            UINT bytes_read = 0;
            UINT bytes_written = 0;            
            f_read(&ofile, main_buffer, main_buffer_size, &bytes_read);
            if (!ShowProgress(pos + (bytes_read / 2), fsize, orig))
                ret = false;
            f_write(&dfile, main_buffer, bytes_read, &bytes_written);
            if (bytes_read != bytes_written)
                ret = false;
        }
        ShowProgress(1, 1, orig);
        
        f_close(&ofile);
        f_close(&dfile);
    }
    
    *(--dname) = '\0';
    return ret;
}

bool PathCopy(const char* destdir, const char* orig) {
    char fdpath[256]; // 256 is the maximum length of a full path
    char fopath[256];
    if (!CheckWritePermissions(destdir)) return false;
    strncpy(fdpath, destdir, 255);
    strncpy(fopath, orig, 255);
    return PathCopyWorker(fdpath, fopath, false);
}

bool PathDeleteWorker(char* fpath) {
    FILINFO fno = {.lfname = NULL};
    
    // this code handles directory content deletion
    if (f_stat(fpath, &fno) != FR_OK) return false; // fpath does not exist
    if (fno.fattrib & AM_DIR) { // process folder contents
        DIR pdir;
        char* fname = fpath + strnlen(fpath, 255);
        
        if (f_opendir(&pdir, fpath) != FR_OK)
            return false;
        *(fname++) = '/';
        fno.lfname = fname;
        fno.lfsize = fpath + 255 - fname;
        
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            if (fname[0] == 0)
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
    if (!CheckWritePermissions(path)) return false;
    strncpy(fpath, path, 256);
    return PathDeleteWorker(fpath);
}

bool PathRename(const char* path, const char* newname) {
    char npath[256]; // 256 is the maximum length of a full path
    char* oldname = strrchr(path, '/');
    if (!CheckWritePermissions(path)) return false;
    if (!oldname) return false;
    oldname++;
    strncpy(npath, path, oldname - path);
    strncpy(npath + (oldname - path), newname, strnlen(newname, 255 - (oldname - path)));
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
    u8* buffer = main_buffer + 54;
    u8* buffer_t = buffer + (400 * 240 * 3);
    char filename[16];
    static u32 n = 0;
    
    for (; n < 1000; n++) {
        snprintf(filename, 16, "0:/snap%03i.bmp", (int) n);
        if (f_stat(filename, NULL) != FR_OK) break;
    }
    if (n >= 1000) return;
    
    memcpy(main_buffer, bmp_header, 54);
    memset(buffer, 0x1F, 400 * 240 * 3 * 2);
    for (u32 x = 0; x < 400; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer_t + (y*400 + x) * 3, TOP_SCREEN0 + (x*240 + y) * 3, 3);
    for (u32 x = 0; x < 320; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer + (y*400 + x + 40) * 3, BOT_SCREEN0 + (x*240 + y) * 3, 3);
    FileCreateData(filename, main_buffer, 54 + (400 * 240 * 3 * 2));
}

void DirEntryCpy(DirEntry* dest, const DirEntry* orig) {
    memcpy(dest, orig, sizeof(DirEntry));
    dest->name = dest->path + (orig->name - orig->path);
}

void SortDirStruct(DirStruct* contents) {
    for (u32 s = 0; s < contents->n_entries; s++) {
        DirEntry* cmp0 = &(contents->entry[s]);
        DirEntry* min0 = cmp0;
        if (cmp0->type == T_VRT_DOTDOT) continue;
        for (u32 c = s + 1; c < contents->n_entries; c++) {
            DirEntry* cmp1 = &(contents->entry[c]);
            if (min0->type != cmp1->type) {
                if (min0->type > cmp1->type)
                    min0 = cmp1;
                continue;
            }
            if (strncasecmp(min0->name, cmp1->name, 256) > 0)
                min0 = cmp1;
        }
        if (min0 != cmp0) {
            DirEntry swap; // swap entries and fix names
            DirEntryCpy(&swap, cmp0);
            DirEntryCpy(cmp0, min0);
            DirEntryCpy(min0, &swap);
        }
    }
}

bool GetRootDirContentsWorker(DirStruct* contents) {
    static const char* drvname[16] = {
        "SDCARD",
        "SYSNAND CTRNAND", "SYSNAND TWLN", "SYSNAND TWLP",
        "EMUNAND CTRNAND", "EMUNAND TWLN", "EMUNAND TWLP"
    };
    u32 n_entries = 0;
    
    for (u32 pdrv = 0; (pdrv < MAX_FS) && (pdrv < MAX_ENTRIES); pdrv++) {
        if (!fs_mounted[pdrv]) continue;
        memset(contents->entry[n_entries].path, 0x00, 16);
        snprintf(contents->entry[n_entries].path + 0,  4, "%lu:", pdrv);
        snprintf(contents->entry[n_entries].path + 4, 32, "[%lu:] %s", pdrv, drvname[pdrv]);
        contents->entry[n_entries].name = contents->entry[n_entries].path + 4;
        contents->entry[n_entries].size = GetTotalSpace(contents->entry[n_entries].path);
        contents->entry[n_entries].type = T_VRT_ROOT;
        contents->entry[n_entries].marked = 0;
        n_entries++;
    }
    contents->n_entries = n_entries;
    
    return contents->n_entries;
}

bool GetDirContentsWorker(DirStruct* contents, char* fpath, int fsize, bool recursive) {
    DIR pdir;
    FILINFO fno;
    char* fname = fpath + strnlen(fpath, fsize - 1);
    bool ret = false;
    
    if (f_opendir(&pdir, fpath) != FR_OK)
        return false;
    (fname++)[0] = '/';
    fno.lfname = fname;
    fno.lfsize = fsize - (fname - fpath);
    
    while (f_readdir(&pdir, &fno) == FR_OK) {
        if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
            continue; // filter out virtual entries
        if (fname[0] == 0)
            strncpy(fname, fno.fname, (fsize - 1) - (fname - fpath));
        if (fno.fname[0] == 0) {
            ret = true;
            break;
        } else {
            DirEntry* entry = &(contents->entry[contents->n_entries]);
            strncpy(entry->path, fpath, 256);
            entry->name = entry->path + (fname - fpath);
            if (fno.fattrib & AM_DIR) {
                entry->type = T_FAT_DIR;
                entry->size = 0;
            } else {
                entry->type = T_FAT_FILE;
                entry->size = fno.fsize;
            }
            entry->marked = 0;
            contents->n_entries++;
            if (contents->n_entries >= MAX_ENTRIES)
                break;
        }
        if (recursive && (fno.fattrib & AM_DIR)) {
            if (!GetDirContentsWorker(contents, fpath, fsize, recursive))
                break;
        }
    }
    f_closedir(&pdir);
    
    return ret;
}

void GetDirContents(DirStruct* contents, const char* path) {
    contents->n_entries = 0;
    if (!(*path)) { // root directory
        if (!GetRootDirContentsWorker(contents))
            contents->n_entries = 0; // not required, but so what?
    } else {
        char fpath[256]; // 256 is the maximum length of a full path
        strncpy(fpath, path, 256);
        // create virtual '..' entry
        contents->entry->name = contents->entry->path + 8;
        strncpy(contents->entry->path, "*?*?*", 8);
        strncpy(contents->entry->name, "..", 4);
        contents->entry->type = T_VRT_DOTDOT;
        contents->entry->size = 0;
        contents->n_entries = 1;
        if (!GetDirContentsWorker(contents, fpath, 256, false))
            contents->n_entries = 0;
        SortDirStruct(contents);
    }
}

uint64_t GetFreeSpace(const char* path)
{
    DWORD free_clusters;
    FATFS *fs_ptr;
    char fsname[4] = { '\0' };
    int fsnum = -1;
    
    strncpy(fsname, path, 2);
    fsnum = *fsname - (int) '0';
    if ((fsnum < 0) || (fsnum >= 7) || (fsname[1] != ':'))
        return -1;
    if (f_getfree(fsname, &free_clusters, &fs_ptr) != FR_OK)
        return -1;

    return (uint64_t) free_clusters * fs[fsnum].csize * _MAX_SS;
}

uint64_t GetTotalSpace(const char* path)
{
    int fsnum = -1;
    
    fsnum = *path - (int) '0';
    if ((fsnum < 0) || (fsnum >= 7) || (path[1] != ':'))
        return -1;
    
    return (uint64_t) (fs[fsnum].n_fatent - 2) * fs[fsnum].csize * _MAX_SS;
}
