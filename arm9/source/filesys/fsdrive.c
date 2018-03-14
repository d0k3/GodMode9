#include "fsdrive.h"
#include "fsgame.h"
#include "fsinit.h"
#include "virtual.h"
#include "sddata.h"
#include "image.h"
#include "ui.h"
#include "vff.h"

// last search pattern, path & mode
static char search_pattern[256] = { 0 };
static char search_path[256] = { 0 };
static bool search_title_mode = false;

int DriveType(const char* path) {
    int type = DRV_UNKNOWN;
    int pdrv = GetMountedFSNum(path);
    
    if (CheckAliasDrive(path)) {
        type = DRV_FAT | DRV_ALIAS | ((*path == 'A') ? DRV_SYSNAND : DRV_EMUNAND);
    } else if (*search_pattern && *search_path && (strncmp(path, "Z:", 3) == 0)) {
        type = DRV_SEARCH;
    } else if ((pdrv >= 0) && (pdrv < NORM_FS)) {
        if (pdrv == 0) {
            type = DRV_FAT | DRV_SDCARD | DRV_STDFAT;
        } else if ((pdrv == 8) && !(GetMountState() & IMG_NAND)) {
            type = DRV_FAT | DRV_SYSNAND | DRV_BONUS | DRV_STDFAT;
        } else if ((pdrv == 9) && !(GetMountState() & IMG_NAND)) {
            type = DRV_FAT | DRV_RAMDRIVE | DRV_STDFAT;
        } else if (pdrv == 1) {
            type = DRV_FAT | DRV_SYSNAND | DRV_CTRNAND | DRV_STDFAT;
        } else if ((pdrv == 2) || (pdrv == 3)) {
            type = DRV_FAT | DRV_SYSNAND | DRV_TWLNAND | DRV_STDFAT;
        } else if (pdrv == 4) {
            type = DRV_FAT | DRV_EMUNAND | DRV_CTRNAND | DRV_STDFAT;
        } else if ((pdrv == 5) || (pdrv == 6)) {
            type = DRV_FAT | DRV_EMUNAND | DRV_TWLNAND | DRV_STDFAT;
        }  else if ((pdrv >= 7) && (pdrv <= 9) &&
            (GetMountState() & (IMG_FAT|IMG_NAND))) {
            type = DRV_FAT | DRV_IMAGE | DRV_STDFAT;
        }    
    } else if (CheckVirtualDrive(path)) {
        int vsrc = GetVirtualSource(path);
        if (vsrc == VRT_SYSNAND) {
            type = DRV_VIRTUAL | DRV_SYSNAND;
        } else if (vsrc == VRT_EMUNAND) {
            type = DRV_VIRTUAL | DRV_EMUNAND;
        } else if (vsrc == VRT_IMGNAND) {
            type = DRV_VIRTUAL | DRV_IMAGE;
        } else if (vsrc == VRT_XORPAD) {
            type = DRV_VIRTUAL | DRV_XORPAD;
        } else if (vsrc == VRT_MEMORY) {
            type = DRV_VIRTUAL | DRV_MEMORY;
        } else if ((vsrc == VRT_GAME) || (vsrc == VRT_KEYDB) || (vsrc == VRT_TICKDB)) {
            type = DRV_VIRTUAL | DRV_GAME | DRV_IMAGE;
        } else if (vsrc == VRT_CART) {
            type = DRV_VIRTUAL | DRV_CART;
        } else if (vsrc == VRT_VRAM) {
            type = DRV_VIRTUAL | DRV_VRAM;
        } 
    }
    
    return type;
}

void SetFSSearch(const char* pattern, const char* path, bool mode) {
    if (pattern && path) {
        strncpy(search_pattern, pattern, 256);
        strncpy(search_path, path, 256);
        search_title_mode = mode;
    } else *search_pattern = *search_path = '\0';
}

bool GetRootDirContentsWorker(DirStruct* contents) {
    static const char* drvname[] = { FS_DRVNAME };
    static const char* drvnum[] = { FS_DRVNUM };
    u32 n_entries = 0;
    
    // virtual root objects hacked in
    for (u32 i = 0; (i < NORM_FS+VIRT_FS) && (n_entries < MAX_DIR_ENTRIES); i++) {
        DirEntry* entry = &(contents->entry[n_entries]);
        if (!DriveType(drvnum[i])) continue; // drive not available
        memset(entry->path, 0x00, 64);
        snprintf(entry->path + 0,  4, drvnum[i]);
        if ((*(drvnum[i]) >= '7') && (*(drvnum[i]) <= '9') && !(GetMountState() & IMG_NAND)) // Drive 7...9 handling
            snprintf(entry->path + 4, 32, "[%s] %s", drvnum[i],
                (*(drvnum[i]) == '7') ? "FAT IMAGE" :
                (*(drvnum[i]) == '8') ? "BONUS DRIVE" :
                (*(drvnum[i]) == '9') ? "RAMDRIVE" : "UNK");
        else if (*(drvnum[i]) == 'G') // Game drive special handling
            snprintf(entry->path + 4, 32, "[%s] %s %s", drvnum[i],
                (GetMountState() & GAME_CIA  ) ? "CIA"   :
                (GetMountState() & GAME_NCSD ) ? "NCSD"  :
                (GetMountState() & GAME_NCCH ) ? "NCCH"  :
                (GetMountState() & GAME_EXEFS) ? "EXEFS" :
                (GetMountState() & GAME_ROMFS) ? "ROMFS" :
                (GetMountState() & GAME_NDS  ) ? "NDS"   :
                (GetMountState() & SYS_FIRM  ) ? "FIRM"  :
                (GetMountState() & GAME_TAD  ) ? "DSIWARE" : "UNK", drvname[i]);
        else snprintf(entry->path + 4, 32, "[%s] %s", drvnum[i], drvname[i]);
        entry->name = entry->path + 4;
        entry->size = GetTotalSpace(entry->path);
        entry->type = T_ROOT;
        entry->marked = 0;
        n_entries++;
    }
    contents->n_entries = n_entries;
    
    return contents->n_entries;
}

bool GetDirContentsWorker(DirStruct* contents, char* fpath, int fnsize, const char* pattern, bool recursive) {
    DIR pdir;
    FILINFO fno;
    char* fname = fpath + strnlen(fpath, fnsize - 1);
    bool ret = false;
    
    if (fvx_opendir(&pdir, fpath) != FR_OK)
        return false;
    if (*(fname-1) != '/') *(fname++) = '/';
    
    while (fvx_readdir(&pdir, &fno) == FR_OK) {
        if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
            continue; // filter out virtual entries
        #ifdef HIDE_HIDDEN
        if (fno.fattrib & AM_HID)
            continue; // filter out hidden entries
        #endif
        strncpy(fname, fno.fname, (fnsize - 1) - (fname - fpath));
        if (fno.fname[0] == 0) {
            ret = true;
            break;
        } else if (!pattern || (fvx_match_name(fname, pattern) == FR_OK)) {
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
            if (!recursive || (entry->type != T_DIR)) {
                if (++(contents->n_entries) >= MAX_DIR_ENTRIES) {
                    ret = true; // Too many entries, still okay if we stop here
                    break;
                }
            }
        }
        if (recursive && (fno.fattrib & AM_DIR)) {
            if (!GetDirContentsWorker(contents, fpath, fnsize, pattern, recursive))
                break;
        }
    }
    fvx_closedir(&pdir);
    
    return ret;
}

void SearchDirContents(DirStruct* contents, const char* path, const char* pattern, bool recursive) {
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
        // search the path
        char fpath[256]; // 256 is the maximum length of a full path
        strncpy(fpath, path, 256);
        if (!GetDirContentsWorker(contents, fpath, 256, pattern, recursive))
            contents->n_entries = 0;
    }
}

void GetDirContents(DirStruct* contents, const char* path) {
    if (*search_path && (DriveType(path) & DRV_SEARCH)) {
        ShowString("Searching, please wait...");
        SearchDirContents(contents, search_path, search_pattern, true);
        if (search_title_mode) SetDirGoodNames(contents);
        ClearScreenF(true, false, COLOR_STD_BG);
    } else SearchDirContents(contents, path, NULL, false);
    if (*path) SortDirStruct(contents);
}

uint64_t GetFreeSpace(const char* path)
{
    DWORD free_clusters;
    FATFS* fsptr;
    char fsname[4] = { '\0' };
    int pdrv = GetMountedFSNum(path);
    FATFS* fsobj = GetMountedFSObject(path);
    if ((pdrv < 0) || !fsobj) return 0;
    
    snprintf(fsname, 3, "%i:", pdrv);
    if (f_getfree(fsname, &free_clusters, &fsptr) != FR_OK)
        return 0;

    return (uint64_t) free_clusters * fsobj->csize * FF_MAX_SS;
}

uint64_t GetTotalSpace(const char* path)
{
    FATFS* fsobj = GetMountedFSObject(path);
    return (fsobj) ? ((uint64_t) (fsobj->n_fatent - 2) * fsobj->csize * FF_MAX_SS) :
        GetVirtualDriveSize(path);
}

uint64_t GetPartitionOffsetSector(const char* path)
{
    FATFS* fsobj = GetMountedFSObject(path);
    return (fsobj) ? (uint64_t) fsobj->volbase : 0;
}
