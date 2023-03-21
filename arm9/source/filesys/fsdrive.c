#include "fsdrive.h"
#include "fsgame.h"
#include "fsinit.h"
#include "language.h"
#include "virtual.h"
#include "vcart.h"
#include "sddata.h"
#include "image.h"
#include "ui.h"
#include "vff.h"

// last search pattern, path & mode
static char search_pattern[256] = { 0 };
static char search_path[256] = { 0 };
static bool title_manager_mode = false;

int DriveType(const char* path) {
    int type = DRV_UNKNOWN;
    int pdrv = GetMountedFSNum(path);

    if (CheckAliasDrive(path)) {
        type = DRV_FAT | DRV_ALIAS | ((*path == 'A') ? DRV_SYSNAND : DRV_EMUNAND);
    } else if (*search_pattern && *search_path && (strncmp(path, "Z:", 3) == 0)) {
        type = DRV_SEARCH;
    } else if (title_manager_mode && (strncmp(path, "Y:", 3) == 0)) {
        type = DRV_VIRTUAL | DRV_TITLEMAN;
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
        } else if ((vsrc == VRT_IMGNAND) || (vsrc == VRT_DISADIFF) || (vsrc == VRT_BDRI)) {
            type = DRV_VIRTUAL | DRV_IMAGE;
        } else if (vsrc == VRT_XORPAD) {
            type = DRV_VIRTUAL | DRV_XORPAD;
        } else if (vsrc == VRT_MEMORY) {
            type = DRV_VIRTUAL | DRV_MEMORY;
        } else if ((vsrc == VRT_GAME) || (vsrc == VRT_KEYDB)) {
            type = DRV_VIRTUAL | DRV_GAME | DRV_IMAGE;
        } else if (vsrc == VRT_CART) {
            type = DRV_VIRTUAL | DRV_CART;
        } else if (vsrc == VRT_VRAM) {
            type = DRV_VIRTUAL | DRV_VRAM;
        }
    }

    return type;
}

void SetFSSearch(const char* pattern, const char* path) {
    if (pattern && path) {
        strncpy(search_pattern, pattern, 256);
        search_pattern[255] = '\0';
        strncpy(search_path, path, 256);
        search_path[255] = '\0';
    } else *search_pattern = *search_path = '\0';
}

void SetTitleManagerMode(bool mode) {
    title_manager_mode = mode;
}

bool GetFATVolumeLabel(const char* drv, char* label) {
    return (f_getlabel(drv, label, NULL) == FR_OK);
}

bool GetRootDirContentsWorker(DirStruct* contents) {
    const char* drvname[] = { FS_DRVNAME };
    static const char* drvnum[] = { FS_DRVNUM };
    u32 n_entries = 0;

    char sdlabel[DRV_LABEL_LEN];
    if (!GetFATVolumeLabel("0:", sdlabel) || !(*sdlabel))
        strcpy(sdlabel, STR_LAB_NOLABEL);

    char carttype[16];
    GetVCartTypeString(carttype);

    // virtual root objects hacked in
    for (u32 i = 0; (i < countof(drvnum)) && (n_entries < MAX_DIR_ENTRIES); i++) {
        DirEntry* entry = &(contents->entry[n_entries]);
        if (!DriveType(drvnum[i])) continue; // drive not available
        entry->p_name = 4;
        entry->name = entry->path + entry->p_name;
        memset(entry->path, 0x00, 256);
        snprintf(entry->path,  4, "%s", drvnum[i]);
        if ((*(drvnum[i]) >= '7') && (*(drvnum[i]) <= '9') && !(GetMountState() & IMG_NAND)) // Drive 7...9 handling
            snprintf(entry->name, 252, "[%s] %s", drvnum[i],
                (*(drvnum[i]) == '7') ? STR_LAB_FAT_IMAGE :
                (*(drvnum[i]) == '8') ? STR_LAB_BONUS_DRIVE :
                (*(drvnum[i]) == '9') ? STR_LAB_RAMDRIVE : "UNK");
        else if (*(drvnum[i]) == 'G') // Game drive special handling
            snprintf(entry->name, 252, "[%s] %s %s", drvnum[i],
                (GetMountState() & GAME_CIA  ) ? "CIA"   :
                (GetMountState() & GAME_NCSD ) ? "NCSD"  :
                (GetMountState() & GAME_NCCH ) ? "NCCH"  :
                (GetMountState() & GAME_EXEFS) ? "EXEFS" :
                (GetMountState() & GAME_ROMFS) ? "ROMFS" :
                (GetMountState() & GAME_NDS  ) ? "NDS"   :
                (GetMountState() & SYS_FIRM  ) ? "FIRM"  :
                (GetMountState() & GAME_TAD  ) ? "DSIWARE" : "UNK", drvname[i]);
        else if (*(drvnum[i]) == 'C') // Game cart handling
            snprintf(entry->name, 252, "[%s] %s (%s)", drvnum[i], drvname[i], carttype);
        else if (*(drvnum[i]) == '0') // SD card handling
            snprintf(entry->name, 252, "[%s] %s (%s)", drvnum[i], drvname[i], sdlabel);
        else snprintf(entry->name, 252, "[%s] %s", drvnum[i], drvname[i]);
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
            if (contents->n_entries >= MAX_DIR_ENTRIES) {
                ret = true; // Too many entries, still okay if we stop here
                break;
            }
            strncpy(entry->path, fpath, 256);
            entry->path[255] = '\0';
            entry->p_name = fname - fpath;
            entry->name = entry->path + entry->p_name;
            if (fno.fattrib & AM_DIR) {
                entry->type = T_DIR;
                entry->size = 0;
            } else {
                entry->type = T_FILE;
                entry->size = fno.fsize;
            }
            entry->marked = 0;
            if (!recursive || (entry->type != T_DIR))
                ++(contents->n_entries);
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
        DirEntry* entry = (DirEntry*) &(contents->entry);
        entry->p_name = 4;
        entry->name = entry->path + entry->p_name;
        strncpy(entry->path, "*?*", 4);
        strncpy(entry->name, "..", 4);
        entry->type = T_DOTDOT;
        entry->size = 0;
        contents->n_entries = 1;
        // search the path
        char fpath[256]; // 256 is the maximum length of a full path
        strncpy(fpath, path, 256);
        fpath[255] = '\0';
        if (!GetDirContentsWorker(contents, fpath, 256, pattern, recursive))
            contents->n_entries = 0;
    }
}

void GetDirContents(DirStruct* contents, const char* path) {
    if (*search_path && (DriveType(path) & DRV_SEARCH)) {
        ShowString("%s", STR_SEARCHING_PLEASE_WAIT);
        SearchDirContents(contents, search_path, search_pattern, true);
        ClearScreenF(true, false, COLOR_STD_BG);
    } else if (title_manager_mode && (DriveType(path) & DRV_TITLEMAN)) {
        SearchDirContents(contents, "T:", "*", false);
        SetupTitleManager(contents);
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

    snprintf(fsname, sizeof(fsname), "%i:", pdrv);
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
