#include "ui.h"
#include "fs.h"
#include "virtual.h"
#include "fatfs/ff.h"

#define MAIN_BUFFER ((u8*)0x21200000)
#define MAIN_BUFFER_SIZE (0x100000) // must be multiple of 0x200

#define NORM_FS  10
#define VIRT_FS  3

// don't use this area for anything else!
static FATFS* fs = (FATFS*)0x20316000; 

// write permission level - careful with this
static u32 write_permission_level = 1;

// number of currently open file systems
static bool fs_mounted[NORM_FS] = { false };

bool InitSDCardFS() {
    #ifndef EXEC_GATEWAY
    // TODO: Magic?
    *(u32*)0x10000020 = 0;
    *(u32*)0x10000020 = 0x340;
    #endif
    fs_mounted[0] = (f_mount(fs, "0:", 1) == FR_OK);
    return fs_mounted[0];
}

bool InitExtFS() {
    if (!fs_mounted[0])
        return false;
    for (u32 i = 1; i < NORM_FS; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (f_mount(fs + i, fsname, 1) != FR_OK) return false;
        fs_mounted[i] = true;
    }
    return true;
}

void DeinitExtFS() {
    for (u32 i = NORM_FS; i > 0; i--) {
        if (fs_mounted[i]) {
            char fsname[8];
            snprintf(fsname, 7, "%lu:", i);
            f_mount(NULL, fsname, 1);
            fs_mounted[i] = false;
        }
    }
}

void DeinitSDCardFS() {
    if (fs_mounted[0]) {
        f_mount(NULL, "0:", 1);
        fs_mounted[0] = false;
    }
}

int PathToNumFS(const char* path) {
    int fsnum = *path - (int) '0';
    if ((fsnum < 0) || (fsnum >= NORM_FS) || (path[1] != ':')) {
        if (!IsVirtualPath(path)) ShowPrompt(false, "Invalid path (%s)", path);
        return -1;
    }
    return fsnum;
}

bool IsMountedFS(const char* path) {
    int fsnum = PathToNumFS(path);
    return ((fsnum >= 0) && (fsnum < NORM_FS)) ? fs_mounted[fsnum] : false;
}

bool CheckWritePermissions(const char* path) {
    int pdrv = PathToNumFS(path);
    if (pdrv < 0) {
        if (IsVirtualPath(path)) // this is a hack, but okay for now
            pdrv = (IsVirtualPath(path) == VRT_SYSNAND) ? 1 : 4; 
        else return false;
    }
    
    if ((pdrv >= 1) && (pdrv <= 3) && (write_permission_level < 3)) {
        if (ShowPrompt(true, "Writing to the SysNAND is locked!\nUnlock it now?"))
            return SetWritePermissions(3);
        return false;
    } else if ((pdrv >= 4) && (pdrv <= 6) && (write_permission_level < 2)) {
        if (ShowPrompt(true, "Writing to the EmuNAND is locked!\nUnlock it now?"))
            return SetWritePermissions(2);
        return false;
    } else if ((pdrv >= 7) && (pdrv <= 9) && (write_permission_level < 2)) {
        if (ShowPrompt(true, "Writing to images is locked!\nUnlock it now?"))
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
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND &\nimage writing permissions.\nKeep backups, just in case."))
                return false;
            break;
        case 3:
            if (!ShowUnlockSequence(3, "!This is your only warning!\n \nYou want to enable SysNAND\nwriting permissions.\nThis enables you to do some\nreally dangerous stuff!\nHaving a SysNAND backup and\nNANDmod is recommended."))
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

bool GetTempFileName(char* path) {
    // this assumes path is initialized with enough room
    char* tempname = strrchr(path, '/');
    if (!tempname) return false;
    tempname++;
    strncpy(tempname, "AAAAAAAA.TMP", 255 - (tempname - path));
    char* cc = tempname;
    // this does not try all permutations
    for (; (*cc <= 'Z') && (cc - tempname < 8); (*cc)++) {
        if (f_stat(path, NULL) != FR_OK) break;
        if (*cc == 'Z') cc++;
    }
    return (cc - tempname < 8) ? true : false;
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

bool PathCopyVirtual(const char* destdir, const char* orig) {
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    char deststr[36 + 1];
    char origstr[36 + 1];
    bool ret = true;
    
    if (oname == NULL) return false; // not a proper origin path
    oname++;
    snprintf(dest, 255, "%s/%s", destdir, oname);
    
    TruncateString(deststr, dest, 36, 8);
    TruncateString(origstr, orig, 36, 8);
    
    if (IsVirtualPath(dest) && IsVirtualPath(orig)) { // virtual to virtual
        VirtualFile dvfile;
        VirtualFile ovfile;
        u32 osize;
        
        if (!FindVirtualFile(&dvfile, dest, 0))
            return false;
        if (!FindVirtualFile(&ovfile, orig, 0))
            return false;
        osize = ovfile.size;
        if (dvfile.size != osize) { // almost impossible, but so what...
            ShowPrompt(false, "Virtual file size mismatch:\n%s\n%s", origstr, deststr);
            return false;
        }
        if ((dvfile.keyslot == ovfile.keyslot) && (dvfile.offset == ovfile.offset)) // this improves copy times
            dvfile.keyslot = ovfile.keyslot = 0xFF;
        
        DeinitExtFS();
        if (!ShowProgress(0, 0, orig)) ret = false;
        for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
            if (ReadVirtualFile(&ovfile, MAIN_BUFFER, pos, read_bytes) != 0)
                ret = false;
            if (!ShowProgress(pos + (read_bytes / 2), osize, orig))
                ret = false;
            if (WriteVirtualFile(&dvfile, MAIN_BUFFER, pos, read_bytes) != 0)
                ret = false;
        }
        ShowProgress(1, 1, orig);
        InitExtFS();
    } else if (IsVirtualPath(dest)) { // SD card to virtual (other FAT not allowed!)
        VirtualFile dvfile;
        FIL ofile;
        u32 osize;
        
        if (f_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return false;
        f_lseek(&ofile, 0);
        f_sync(&ofile);
        osize = f_size(&ofile);
        if (!FindVirtualFile(&dvfile, dest, 0)) {
            if (!FindVirtualFile(&dvfile, dest, osize)) {
                f_close(&ofile);
                return false;
            }
            snprintf(dest, 255, "%s/%s", destdir, dvfile.name);
            if (!ShowPrompt(true, "Entry not found: %s\nInject into %s instead?", deststr, dest)) {
                f_close(&ofile);
                return false;
            }
            TruncateString(deststr, dest, 36, 8);
        }
        if (dvfile.size != osize) {
            char osizestr[32];
            char dsizestr[32];
            FormatBytes(osizestr, osize);
            FormatBytes(dsizestr, dvfile.size);
            if (dvfile.size > osize) {
                if (!ShowPrompt(true, "File smaller than available space:\n%s (%s)\n%s (%s)\nContinue?", origstr, osizestr, deststr, dsizestr)) {
                    f_close(&ofile);
                    return false;
                }
            } else {
                ShowPrompt(false, "File bigger than available space:\n%s (%s)\n%s (%s)", origstr, osizestr, deststr, dsizestr);
                f_close(&ofile);
                return false;
            }
        }
        
        DeinitExtFS();
        if (!ShowProgress(0, 0, orig)) ret = false;
        for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT bytes_read = 0;           
            if (f_read(&ofile, MAIN_BUFFER, MAIN_BUFFER_SIZE, &bytes_read) != FR_OK)
                ret = false;
            if (!ShowProgress(pos + (bytes_read / 2), osize, orig))
                ret = false;
            if (WriteVirtualFile(&dvfile, MAIN_BUFFER, pos, bytes_read) != 0)
                ret = false;
        }
        ShowProgress(1, 1, orig);
        f_close(&ofile);
        InitExtFS();
    } else if (IsVirtualPath(orig)) { // virtual to any file system
        VirtualFile ovfile;
        FIL dfile;
        u32 osize;
        
        if (!FindVirtualFile(&ovfile, orig, 0))
            return false;
        // check if destination exists
        if (f_stat(dest, NULL) == FR_OK) {
            if (!ShowPrompt(true, "Destination already exists:\n%s\nOverwrite existing file(s)?", deststr))
                return false;
        }
        if (f_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
            return false;
        f_lseek(&dfile, 0);
        f_sync(&dfile);
        osize = ovfile.size;
        if (GetFreeSpace(dest) < osize) {
            ShowPrompt(false, "Error: File is too big for destination");
            f_close(&dfile);
            return false;
        }
        
        if (!ShowProgress(0, 0, orig)) ret = false;
        for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
            UINT bytes_written = 0;
            if (ReadVirtualFile(&ovfile, MAIN_BUFFER, pos, read_bytes) != 0)
                ret = false;
            if (!ShowProgress(pos + (read_bytes / 2), osize, orig))
                ret = false;
            if (f_write(&dfile, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK)
                ret = false;
            if (read_bytes != bytes_written)
                ret = false;
        }
        ShowProgress(1, 1, orig);
        f_close(&dfile);
    } else {
        return false;
    }
    
    return ret;
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
        for (size_t pos = 0; (pos < fsize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT bytes_read = 0;
            UINT bytes_written = 0;            
            if (f_read(&ofile, MAIN_BUFFER, MAIN_BUFFER_SIZE, &bytes_read) != FR_OK)
                ret = false;
            if (!ShowProgress(pos + (bytes_read / 2), fsize, orig))
                ret = false;
            if (f_write(&dfile, MAIN_BUFFER, bytes_read, &bytes_written) != FR_OK)
                ret = false;
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
    if (!CheckWritePermissions(destdir)) return false;
    if (IsVirtualPath(destdir) || IsVirtualPath(orig)) {
        // users are inventive...
        if ((PathToNumFS(orig) > 0) && IsVirtualPath(destdir)) {
            ShowPrompt(false, "Only files from SD card are accepted");
            return false;
        }
        return PathCopyVirtual(destdir, orig);
    } else {
        char fdpath[256]; // 256 is the maximum length of a full path
        char fopath[256];
        strncpy(fdpath, destdir, 255);
        strncpy(fopath, orig, 255);
        return PathCopyWorker(fdpath, fopath, false);
    }
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
    strncpy(npath + (oldname - path), newname, 255 - (oldname - path));
    
    FRESULT res = f_rename(path, npath);
    if (res == FR_EXIST) { // new path already exists (possible LFN/case issue)
        char temp[256];
        strncpy(temp, path, oldname - path);
        if (!GetTempFileName(temp)) return false;
        if (f_rename(path, temp) == FR_OK) {
            if ((f_stat(npath, NULL) == FR_OK) || (f_rename(temp, npath) != FR_OK)) {
                ShowPrompt(false, "Destination exists in folder");
                f_rename(temp, path); // something went wrong - try renaming back
                return false;
            } else return true;
        } else return false;
    } else return (res == FR_OK) ? true : false;
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
        if (f_stat(filename, NULL) != FR_OK) break;
    }
    if (n >= 1000) return;
    
    memcpy(MAIN_BUFFER, bmp_header, 54);
    memset(buffer, 0x1F, 400 * 240 * 3 * 2);
    for (u32 x = 0; x < 400; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer_t + (y*400 + x) * 3, TOP_SCREEN0 + (x*240 + y) * 3, 3);
    for (u32 x = 0; x < 320; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer + (y*400 + x + 40) * 3, BOT_SCREEN0 + (x*240 + y) * 3, 3);
    FileCreateData(filename, MAIN_BUFFER, 54 + (400 * 240 * 3 * 2));
}

void DirEntryCpy(DirEntry* dest, const DirEntry* orig) {
    memcpy(dest, orig, sizeof(DirEntry));
    dest->name = dest->path + (orig->name - orig->path);
}

void SortDirStruct(DirStruct* contents) {
    for (u32 s = 0; s < contents->n_entries; s++) {
        DirEntry* cmp0 = &(contents->entry[s]);
        DirEntry* min0 = cmp0;
        if (cmp0->type == T_DOTDOT) continue;
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
    static const char* drvname[] = {
        "SDCARD",
        "SYSNAND CTRNAND", "SYSNAND TWLN", "SYSNAND TWLP",
        "EMUNAND CTRNAND", "EMUNAND TWLN", "EMUNAND TWLP",
        "IMGNAND CTRNAND", "IMGNAND TWLN", "IMGNAND TWLP",
        "SYSNAND VIRTUAL", "EMUNAND VIRTUAL", "IMGNAND VIRTUAL",
    };
    static const char* drvnum[] = {
        "0:", "1:", "2:", "3:", "4:", "5:", "6:", "7:", "8:", "9:", "S:", "E:", "I:"
    };
    u32 n_entries = 0;
    
    // virtual root objects hacked in
    for (u32 pdrv = 0; (pdrv < NORM_FS+VIRT_FS) && (n_entries < MAX_ENTRIES); pdrv++) {
        DirEntry* entry = &(contents->entry[n_entries]);
        if ((pdrv < NORM_FS) && !fs_mounted[pdrv]) continue;
        else if ((pdrv >= NORM_FS) && (!CheckVirtualPath(drvnum[pdrv]))) continue;
        memset(entry->path, 0x00, 64);
        snprintf(entry->path + 0,  4, drvnum[pdrv]);
        snprintf(entry->path + 4, 32, "[%s] %s", drvnum[pdrv], drvname[pdrv]);
        entry->name = entry->path + 4;
        entry->size = GetTotalSpace(entry->path);
        entry->type = T_ROOT;
        entry->marked = 0;
        n_entries++;
    }
    contents->n_entries = n_entries;
    
    return contents->n_entries;
}

bool GetVirtualDirContentsWorker(DirStruct* contents, const char* path) {
    if (strchr(path, '/')) return false; // only top level paths
    for (u32 n = 0; (n < virtualFileList_size) && (contents->n_entries < MAX_ENTRIES); n++) {
        VirtualFile vfile;
        DirEntry* entry = &(contents->entry[contents->n_entries]);
        snprintf(entry->path, 256, "%s/%s", path, virtualFileList[n]);
        if (!FindVirtualFile(&vfile, entry->path, 0)) continue;
        entry->name = entry->path + strnlen(path, 256) + 1;
        entry->size = vfile.size;
        entry->type = T_FILE;
        entry->marked = 0;
        contents->n_entries++;
    }
    
    return true; // not much we can check here
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
                entry->type = T_DIR;
                entry->size = 0;
            } else {
                entry->type = T_FILE;
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
        // create virtual '..' entry
        contents->entry->name = contents->entry->path + 8;
        strncpy(contents->entry->path, "*?*?*", 8);
        strncpy(contents->entry->name, "..", 4);
        contents->entry->type = T_DOTDOT;
        contents->entry->size = 0;
        contents->n_entries = 1;
        if (IsVirtualPath(path)) {
            if (!GetVirtualDirContentsWorker(contents, path))
                contents->n_entries = 0;
        } else {
            char fpath[256]; // 256 is the maximum length of a full path
            strncpy(fpath, path, 256);
            if (!GetDirContentsWorker(contents, fpath, 256, false))
                contents->n_entries = 0;
        }
        SortDirStruct(contents);
    }
}

uint64_t GetFreeSpace(const char* path)
{
    DWORD free_clusters;
    FATFS *fs_ptr;
    char fsname[4] = { '\0' };
    int pdrv = PathToNumFS(path);
    if (pdrv < 0) return 0;
    
    snprintf(fsname, 3, "%i:", pdrv);
    if (f_getfree(fsname, &free_clusters, &fs_ptr) != FR_OK)
        return 0;

    return (uint64_t) free_clusters * fs[pdrv].csize * _MAX_SS;
}

uint64_t GetTotalSpace(const char* path)
{
    int pdrv = PathToNumFS(path);
    if (pdrv < 0) return 0;
    
    return (uint64_t) (fs[pdrv].n_fatent - 2) * fs[pdrv].csize * _MAX_SS;
}

uint64_t GetPartitionOffsetSector(const char* path)
{
    int pdrv = PathToNumFS(path);
    if (pdrv < 0) return -1;
    
    return (uint64_t) fs[pdrv].volbase;
}
