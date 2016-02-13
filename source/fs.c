#include "fs.h"
#include "draw.h"

#include "fatfs/ff.h"

static FATFS fs;
static FIL file;
static DIR dir;

bool InitFS()
{
#ifndef EXEC_GATEWAY
    // TODO: Magic?
    *(u32*)0x10000020 = 0;
    *(u32*)0x10000020 = 0x340;
#endif
    bool ret = (f_mount(&fs, "0:", 0) == FR_OK);
#ifdef WORK_DIR
    f_chdir(WORK_DIR);
#endif
    return ret;
}

void DeinitFS()
{
    LogWrite(NULL);
    f_mount(NULL, "0:", 1);
}

bool FileOpen(const char* path)
{
    unsigned flags = FA_READ | FA_WRITE | FA_OPEN_EXISTING;
    if (*path == '/')
        path++;
    bool ret = (f_open(&file, path, flags) == FR_OK);
    #ifdef WORK_DIR
    f_chdir("/"); // temporarily change the current directory
    if (!ret) ret = (f_open(&file, path, flags) == FR_OK);
    f_chdir(WORK_DIR);
    #endif
    f_lseek(&file, 0);
    f_sync(&file);
    return ret;
}

bool DebugFileOpen(const char* path)
{
    Debug("Opening %s ...", path);
    if (!FileOpen(path)) {
        Debug("Could not open %s!", path);
        return false;
    }
    
    return true;
}

bool FileCreate(const char* path, bool truncate)
{
    unsigned flags = FA_READ | FA_WRITE;
    flags |= truncate ? FA_CREATE_ALWAYS : FA_OPEN_ALWAYS;
    if (*path == '/')
        path++;
    bool ret = (f_open(&file, path, flags) == FR_OK);
    f_lseek(&file, 0);
    f_sync(&file);
    return ret;
}

bool DebugFileCreate(const char* path, bool truncate) {
    Debug("Creating %s ...", path);
    if (!FileCreate(path, truncate)) {
        Debug("Could not create %s!", path);
        return false;
    }

    return true;
}

size_t FileCopyTo(const char* dest, void* buf, size_t bufsize)
{
    unsigned flags = FA_READ | FA_WRITE | FA_CREATE_ALWAYS;
    size_t fsize = f_size(&file);
    size_t result = fsize;
    FIL dfile;
    // make sure the containing folder exists
    char tmp[256] = { 0 };
    strncpy(tmp, dest, sizeof(tmp) - 1);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            char s = *p;
            *p = 0;
            f_mkdir(tmp);
            *p = s;
        }
    }
    // do the actual copying
    if (f_open(&dfile, dest, flags) != FR_OK)
        return 0;
    f_lseek(&dfile, 0);
    f_sync(&dfile);
    f_lseek(&file, 0);
    f_sync(&file);
    for (size_t pos = 0; pos < fsize; pos += bufsize) {
        UINT bytes_read = 0;
        UINT bytes_written = 0;
        ShowProgress(pos, fsize);
        f_read(&file, buf, bufsize, &bytes_read);
        f_write(&dfile, buf, bytes_read, &bytes_written);
        if (bytes_read != bytes_written) {
            result = 0;
        }
    }
    ShowProgress(0, 0);
    f_close(&dfile);
    return result;
}

size_t FileRead(void* buf, size_t size, size_t foffset)
{
    UINT bytes_read = 0;
    f_lseek(&file, foffset);
    f_read(&file, buf, size, &bytes_read);
    return bytes_read;
}

bool DebugFileRead(void* buf, size_t size, size_t foffset) {
    size_t bytesRead = FileRead(buf, size, foffset);
    if(bytesRead != size) {
        Debug("ERROR, file is too small!");
        return false;
    }
    
    return true;
}

size_t FileWrite(void* buf, size_t size, size_t foffset)
{
    UINT bytes_written = 0;
    f_lseek(&file, foffset);
    f_write(&file, buf, size, &bytes_written);
    f_sync(&file);
    return bytes_written;
}

bool DebugFileWrite(void* buf, size_t size, size_t foffset)
{
    size_t bytesWritten = FileWrite(buf, size, foffset);
    if(bytesWritten != size) {
        Debug("ERROR, SD card may be full!");
        return false;
    }
    
    return true;
}

size_t FileGetSize()
{
    return f_size(&file);
}

void FileClose()
{
    f_close(&file);
}

bool DirMake(const char* path)
{
    FRESULT res = f_mkdir(path);
    bool ret = (res == FR_OK) || (res == FR_EXIST);
    return ret;
}

bool DebugDirMake(const char* path)
{
    Debug("Creating dir %s ...", path);
    if (!DirMake(path)) {
        Debug("Could not create %s!", path);
        return false;
    }
    
    return true;
}

bool DirOpen(const char* path)
{
    return (f_opendir(&dir, path) == FR_OK);
}

bool DebugDirOpen(const char* path)
{
    Debug("Opening %s ...", path);
    if (!DirOpen(path)) {
        Debug("Could not open %s!", path);
        return false;
    }
    
    return true;
}

bool DirRead(char* fname, int fsize)
{
    FILINFO fno;
    fno.lfname = fname;
    fno.lfsize = fsize;
    bool ret = false;
    while (f_readdir(&dir, &fno) == FR_OK) {
        if (fno.fname[0] == 0) break;
        if ((fno.fname[0] != '.') && !(fno.fattrib & AM_DIR)) {
            if (fname[0] == 0)
                strcpy(fname, fno.fname);
            ret = true;
            break;
        }
    }
    return ret;
}

void DirClose()
{
    f_closedir(&dir);
}

bool GetFileListWorker(char** list, int* lsize, char* fpath, int fsize, bool recursive, bool inc_files, bool inc_dirs)
{
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
        } else if ((inc_files && !(fno.fattrib & AM_DIR)) || (inc_dirs && (fno.fattrib & AM_DIR))) {
            snprintf(*list, *lsize, "%s\n", fpath);
            for(;(*list)[0] != '\0' && (*lsize) > 1; (*list)++, (*lsize)--); 
            if ((*lsize) <= 1) break;
        }
        if (recursive && (fno.fattrib & AM_DIR)) {
            if (!GetFileListWorker(list, lsize, fpath, fsize, recursive, inc_files, inc_dirs))
                break;
        }
    }
    f_closedir(&pdir);
    
    return ret;
}

bool GetFileList(const char* path, char* list, int lsize, bool recursive, bool inc_files, bool inc_dirs)
{
    char fpath[256]; // 256 is the maximum length of a full path
    strncpy(fpath, path, 256);
    return GetFileListWorker(&list, &lsize, fpath, 256, recursive, inc_files, inc_dirs);
}

size_t LogWrite(const char* text)
{
    #ifdef LOG_FILE
    static FIL lfile;
    static bool lready = false;
    static size_t lstart = 0;
    
    if ((text == NULL) && lready) {
        f_sync(&lfile);
        f_close(&lfile);
        lready = false;
        return lstart; // return the current log start
    } else if (text == NULL) {
        return 0;
    }
    
    if (!lready) {
        unsigned flags = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;
        lready = (f_open(&lfile, LOG_FILE, flags) == FR_OK);
        if (!lready) return 0;
        lstart = f_size(&lfile);
        f_lseek(&lfile, lstart);
        f_sync(&lfile);
    }
    
    const char newline = '\n';
    UINT bytes_written;
    UINT tlen = strnlen(text, 128); 
    f_write(&lfile, text, tlen, &bytes_written);
    if (bytes_written != tlen) return 0;
    f_write(&lfile, &newline, 1, &bytes_written);
    if (bytes_written != 1) return 0;
    
    return f_size(&lfile); // return the current position
    #else
    return 0;
    #endif
}

static uint64_t ClustersToBytes(FATFS* fs, DWORD clusters)
{
    uint64_t sectors = clusters * fs->csize;
#if _MAX_SS != _MIN_SS
    return sectors * fs->ssize;
#else
    return sectors * _MAX_SS;
#endif
}

uint64_t RemainingStorageSpace()
{
    DWORD free_clusters;
    FATFS *fs2;
    FRESULT res = f_getfree("0:", &free_clusters, &fs2);
    if (res)
        return -1;

    return ClustersToBytes(&fs, free_clusters);
}

uint64_t TotalStorageSpace()
{
    return ClustersToBytes(&fs, fs.n_fatent - 2);
}
