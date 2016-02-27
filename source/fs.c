#include "draw.h"
#include "fs.h"
#include "fatfs/ff.h"

// don't use this area for anything else!
static FATFS* fs = (FATFS*)0x20316000; 
// reserve one MB for this, just to be safe
static DirStruct* curdir_contents = (DirStruct*)0x21000000;
// this is the main buffer
static u8* main_buffer = (u8*)0x21100000;
// number of currently open file systems
static u32 numfs = 0;

bool InitFS()
{
    #ifndef EXEC_GATEWAY
    // TODO: Magic?
    *(u32*)0x10000020 = 0;
    *(u32*)0x10000020 = 0x340;
    #endif
    for (numfs = 0; numfs < 16; numfs++) {
        char fsname[8];
        snprintf(fsname, 8, "%lu:", numfs);
        int res = f_mount(fs + numfs, fsname, 1);
        if (res != FR_OK) {
            if (numfs >= 4) break;
            ShowError("Initialising failed! (%lu/%s/%i)", numfs, fsname, res);
            DeinitFS();
            return false;
        }
    }
    ShowError("Mounted: %i partitions", numfs);
    return true;
}

void DeinitFS()
{
    for (u32 i = 0; i < numfs; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", numfs);
        f_mount(NULL, fsname, 1);
    }
    numfs = 0;
}

bool FileExists(const char* path) {
    return (f_stat(path, NULL) == FR_OK);
}

bool FileCreate(const char* path, u8* data, u32 size) {
    FIL file;
    UINT bytes_written = 0;
    if (f_open(&file, path, FA_READ | FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;
    f_write(&file, data, size, &bytes_written);
    f_close(&file);
    return (bytes_written == size);
}

void Screenshot()
{
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
        if (!FileExists(filename)) break;
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
    FileCreate(filename, main_buffer, 54 + (400 * 240 * 3 * 2));
}

bool GetRootDirContentsWorker(DirStruct* contents) {
    static const char* drvname[16] = {
        "SDCARD",
        "SYSCTRN", "SYSTWLN", "SYSTWLP",
        "EMU0CTRN", "EMU0TWLN", "EMU0TWLP",
        "EMU1CTRN", "EMU1TWLN", "EMU1TWLP",
        "EMU2CTRN", "EMU2TWLN", "EMU2TWLP",
        "EMU3CTRN", "EMU3TWLN", "EMU3TWLP"
    };
    
    for (u32 pdrv = 0; (pdrv < numfs) && (pdrv < MAX_ENTRIES); pdrv++) {
        memset(contents->entry[pdrv].path, 0x00, 16);
        snprintf(contents->entry[pdrv].path + 0,  4, "%lu:", pdrv);
        snprintf(contents->entry[pdrv].path + 4, 16, "[%lu:] %s", pdrv, drvname[pdrv]);
        contents->entry[pdrv].name = contents->entry[pdrv].path + 4;
        contents->entry[pdrv].size = 0;
        contents->entry[pdrv].type = T_FAT_DIR;
    }
    contents->n_entries = numfs;
    
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

DirStruct* GetDirContents(const char* path) {
    curdir_contents->n_entries = 0;
    if (strncmp(path, "", 256) == 0) { // root directory
        if (!GetRootDirContentsWorker(curdir_contents))
            curdir_contents->n_entries = 0; // not required, but so what?
    } else {
        char fpath[256]; // 256 is the maximum length of a full path
        strncpy(fpath, path, 256);
        if (!GetDirContentsWorker(curdir_contents, fpath, 256, false))
            curdir_contents->n_entries = 0;
    }
    
    return curdir_contents;
}

/*static uint64_t ClustersToBytes(FATFS* fs, DWORD clusters)
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
}*/
