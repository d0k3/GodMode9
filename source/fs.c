#include "ui.h"
#include "fs.h"
#include "virtual.h"
#include "vgame.h"
#include "sddata.h"
#include "image.h"
#include "sha.h"
#include "sdmmc.h"
#include "ff.h"

#define NORM_FS  10
#define VIRT_FS  8

#define SKIP_CUR (1<<3)
#define OVERWRITE_CUR (1<<4)

// Volume2Partition resolution table
PARTITION VolToPart[] = {
    {0, 1}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
    {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}
};

// don't use this area for anything else!
static FATFS* fs = (FATFS*)0x20316000; 

// write permissions - careful with this
static u32 write_permissions = PERM_BASE;

// number of currently open file systems
static bool fs_mounted[NORM_FS] = { false };

// last search pattern & path
static char search_pattern[256] = { 0 };
static char search_path[256] = { 0 };

bool InitSDCardFS() {
    fs_mounted[0] = (f_mount(fs, "0:", 1) == FR_OK);
    return fs_mounted[0];
}

bool InitExtFS() {
    u32 mount_state = GetMountState();
    u32 last_fs = (mount_state == IMG_NAND) ? NORM_FS :
        ((mount_state == IMG_FAT) || (mount_state == IMG_RAMDRV)) ? NORM_FS - 2 : NORM_FS - 3;
    if (!fs_mounted[0])
        return false;
    for (u32 i = 1; i < last_fs; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (fs_mounted[i]) continue;
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
        if ((i == 7) && !fs_mounted[7] && (mount_state == IMG_RAMDRV)) {
            f_mkfs("7:", FM_ANY, 0, MAIN_BUFFER, MAIN_BUFFER_SIZE); // format ramdrive if required
            f_mount(NULL, fsname, 1);
            fs_mounted[7] = (f_mount(fs + 7, "7:", 1) == FR_OK);
        }
    }
    SetupNandSdDrive("A:", "0:", "1:/private/movable.sed", 0);
    SetupNandSdDrive("B:", "0:", "4:/private/movable.sed", 1);
    return true;
}

void DeinitExtFS() {
    SetupNandSdDrive(NULL, NULL, NULL, 0);
    SetupNandSdDrive(NULL, NULL, NULL, 1);
    for (u32 i = NORM_FS - 1; i > 0; i--) {
        if (fs_mounted[i]) {
            char fsname[8];
            snprintf(fsname, 7, "%lu:", i);
            f_mount(NULL, fsname, 1);
            fs_mounted[i] = false;
        }
        if ((i == 7) && (GetMountState() != IMG_RAMDRV)) { // unmount image
            MountImage(NULL);
            InitVGameDrive();
        }
    }
}

void DeinitSDCardFS() {
    if (GetMountState() != IMG_RAMDRV) {
        MountImage(NULL);
        InitVGameDrive();
    }
    if (fs_mounted[0]) {
        f_mount(NULL, "0:", 1);
        fs_mounted[0] = false;
    }
}

void SetFSSearch(const char* pattern, const char* path) {
    if (pattern && path) {
        strncpy(search_pattern, pattern, 256);
        strncpy(search_path, path, 256);
    } else *search_pattern = *search_path = '\0';
}

int PathToNumFS(const char* path) {
    int fsnum = *path - (int) '0';
    if ((fsnum < 0) || (fsnum >= NORM_FS) || (path[1] != ':')) {
        // this check is not required
        /* if (!GetVirtualSource(path) &&
            !CheckAliasDrive(path) &&
            !IsSearchDrive(path))
            ShowPrompt(false, "Invalid path (%s)", path); */
        return -1;
    }
    return fsnum;
}

int PathToNumFSA(const char* path) {
    char alias[256];
    dealias_path(alias, path);
    return PathToNumFS(alias);
}

bool IsSearchDrive(const char* path) {
    return *search_pattern && *search_path && (strncmp(path, "Z:", 3) == 0);
}

int DriveType(const char* path) {
    int type = DRV_UNKNOWN;
    int pdrv = PathToNumFS(path);
    
    if ((pdrv >= 0) && (pdrv < NORM_FS)) {
        if (!fs_mounted[pdrv]) {
            type = DRV_UNKNOWN;
        } else if (pdrv == 0) {
            type = DRV_FAT | DRV_SDCARD | DRV_STDFAT;
        } else if ((pdrv == 7) && (GetMountState() == IMG_RAMDRV)) {
            type = DRV_FAT | DRV_RAMDRIVE | DRV_STDFAT;
        } else if ((pdrv >= 1) && (pdrv <= 3)) {
            type = DRV_FAT | DRV_SYSNAND | DRV_STDFAT;
        } else if ((pdrv >= 4) && (pdrv <= 6)) {
            type = DRV_FAT | DRV_EMUNAND | DRV_STDFAT;
        }  else if ((pdrv >= 7) && (pdrv <= 9) &&
            ((GetMountState() == IMG_FAT) || (GetMountState() == IMG_NAND))) {
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
        } else if (vsrc == VRT_MEMORY) {
            type = DRV_VIRTUAL | DRV_MEMORY;
        } else if (vsrc == VRT_GAME) {
            type = DRV_VIRTUAL | DRV_GAME | DRV_IMAGE;
        } 
    } else if (CheckAliasDrive(path)) {
        type = DRV_FAT | DRV_ALIAS;
    } else if (IsSearchDrive(path)) {
        type = DRV_SEARCH;
    }
    
    return type;
}

uint64_t GetSDCardSize() {
    if (sdmmc_sdcard_init() != 0) return 0;
    return (u64) getMMCDevice(1)->total_size * 512;
}

bool FormatSDCard(u64 hidden_mb, u32 cluster_size) {
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
        ShowPrompt(false, "ERROR: SD card is too small");
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
    if (!ShowUnlockSequence(3, "!WARNING!\n \nProceeding will format this SD.\nThis will irreversibly delete\nALL data on it.\n"))
        return false;
    ShowString("Formatting SD, please wait..."); 
    
    // write the MBR to disk
    // !this assumes a fully deinitialized file system!
    if ((sdmmc_sdcard_init() != 0) || (sdmmc_sdcard_writesectors(0, 1, mbr) != 0)) {
        ShowPrompt(false, "ERROR: SD card i/o failure");
        return false;
    }
    
    // format the SD card
    f_mount(fs, "0:", 1);
    UINT c_size = cluster_size;
    bool ret = (f_mkfs("0:", FM_FAT32, c_size, MAIN_BUFFER, MAIN_BUFFER_SIZE) == FR_OK) && (f_setlabel("0:GM9SD") == FR_OK);
    f_mount(NULL, "0:", 1);
    
    return ret;
}

bool CheckWritePermissions(const char* path) {
    char area_name[16];
    int drvtype = DriveType(path);
    u32 perm;
    
    // check mounted image write permissions
    if ((drvtype & DRV_IMAGE) && !CheckWritePermissions(GetMountPath()))
        return false; // endless loop when mounted file inside image, but not possible
    
    // check drive type, get permission type
    if (drvtype & DRV_SYSNAND) {
        perm = PERM_SYSNAND;
        snprintf(area_name, 16, "the SysNAND");
        // check virtual file flags (if any)
        VirtualFile vfile;
        if (GetVirtualFile(&vfile, path) && (vfile.flags & VFLAG_A9LH_AREA)) {
            perm = PERM_A9LH;
            snprintf(area_name, 16, "A9LH regions");
        }
    } else if (drvtype & DRV_EMUNAND) {
        perm = PERM_EMUNAND;
        snprintf(area_name, 16, "the EmuNAND");
    } else if (drvtype & DRV_GAME) {
        perm = PERM_GAME;
        snprintf(area_name, 16, "game images");
    } else if (drvtype & DRV_IMAGE) {
        perm = PERM_IMAGE;
        snprintf(area_name, 16, "images");
    } else if (drvtype & DRV_MEMORY) {
        perm = PERM_MEMORY;
        snprintf(area_name, 16, "memory areas");
    } else if ((drvtype & DRV_ALIAS) || (strncmp(path, "0:/Nintendo 3DS", 15) == 0)) {
        perm = PERM_SDDATA;
        snprintf(area_name, 16, "SD system data");
    } else if (drvtype & DRV_SDCARD) {
        perm = PERM_SDCARD;
        snprintf(area_name, 16, "the SD card");
    } else if (drvtype & DRV_RAMDRIVE) {
        perm = PERM_RAMDRIVE;
        snprintf(area_name, 16, "the RAM drive");
    } else {
        return false;
    }
    
    // check permission, return if already set
    if ((write_permissions & perm) == perm)
        return true;
    
    // ask the user
    if (!ShowPrompt(true, "Writing to %s is locked!\nUnlock it now?", area_name))
        return false;
        
    return SetWritePermissions(perm, true);
}

bool SetWritePermissions(u32 perm, bool add_perm) {
    if ((write_permissions & perm) == perm) { // write permissions already given
        if (!add_perm) write_permissions = perm;
        return true;
    }
    
    switch (perm) {
        case PERM_BASE:
            if (!ShowUnlockSequence(1, "You want to enable base\nwriting permissions."))
                return false;
            break;
        case PERM_SDCARD:
            if (!ShowUnlockSequence(1, "You want to enable SD card\nwriting permissions."))
                return false;
            break;
        case PERM_RAMDRIVE:
            if (!ShowUnlockSequence(1, "You want to enable RAM drive\nwriting permissions."))
                return false;
        case PERM_EMUNAND:
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND\nwriting permissions."))
                return false;
            break;
        case PERM_IMAGE:
            if (!ShowUnlockSequence(2, "You want to enable image\nwriting permissions."))
                return false;
            break;
        case PERM_GAME:
            ShowPrompt(false, "Unlock write permission for\ngame images is not allowed.");
            return false;
            break;
        #ifndef SAFEMODE
        case PERM_SYSNAND:
            if (!ShowUnlockSequence(3, "!Better be careful!\n \nYou want to enable SysNAND\nwriting permissions.\nThis enables you to do some\nreally dangerous stuff!"))
                return false;
            break;
        case PERM_A9LH:
            if (!ShowUnlockSequence(5, "!THIS IS YOUR ONLY WARNING!\n \nYou want to enable A9LH area\nwriting permissions.\nThis enables you to OVERWRITE\nyour A9LH installation!"))
                return false;
            break;
        case PERM_MEMORY:
            if (!ShowUnlockSequence(4, "!Better be careful!\n \nYou want to enable memory\nwriting permissions.\nWriting to certain areas may\nlead to unexpected results."))
                return false;
            break;
        case PERM_SDDATA:
            if (!ShowUnlockSequence(2, "You want to enable SD data\nwriting permissions."))
                return false;
            break;
        case PERM_ALL:
            if (!ShowUnlockSequence(3, "!Better be careful!\n \nYou want to enable ALL\nwriting permissions.\nThis enables you to do some\nreally dangerous stuff!"))
                return false;
            break;
        default:
            ShowPrompt(false, "Unlock write permission is not allowed.");
            return false;
            break;
        #else
        case PERM_ALL:
            perm &= ~(PERM_SYSNAND|PERM_MEMORY);
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND &\nimage writing permissions.\nKeep backups, just in case."))
                return false;
            break;
        default:
            ShowPrompt(false, "Can't unlock write permission.\nTry GodMode9 instead!");
            return false;
            break;
        #endif
    }
    
    write_permissions = add_perm ? write_permissions | perm : perm;
    
    return true;
}

u32 GetWritePermissions() {
    return write_permissions;
}

bool FileSetData(const char* path, const u8* data, size_t size, size_t foffset, bool create) {
    int drvtype = DriveType(path);
    if (!CheckWritePermissions(path)) return false;
    if (drvtype & DRV_FAT) {
        UINT bytes_written = 0;
        FIL file;
        if (fx_open(&file, path, FA_WRITE | (create ? FA_CREATE_ALWAYS : FA_OPEN_ALWAYS)) != FR_OK)
            return false;
        f_lseek(&file, foffset);
        fx_write(&file, data, size, &bytes_written);
        fx_close(&file);
        return (bytes_written == size);
    } else if (drvtype & DRV_VIRTUAL) {
        VirtualFile vfile;
        if (!GetVirtualFile(&vfile, path))
            return 0;
        return (WriteVirtualFile(&vfile, data, foffset, size, NULL) == 0);
    }
    return false;
}

size_t FileGetData(const char* path, u8* data, size_t size, size_t foffset) {
    int drvtype = DriveType(path);
    if (drvtype & DRV_FAT) {
        UINT bytes_read = 0;
        FIL file;
        if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return 0;
        f_lseek(&file, foffset);
        if (fx_read(&file, data, size, &bytes_read) != FR_OK) {
            fx_close(&file);
            return 0;
        }
        fx_close(&file);
        return bytes_read;
    } else if (drvtype & DRV_VIRTUAL) {
        u32 bytes_read = 0;
        VirtualFile vfile;
        if (!GetVirtualFile(&vfile, path))
            return 0;
        return (ReadVirtualFile(&vfile, data, foffset, size, &bytes_read) == 0) ? bytes_read : 0;
    }
    return 0;
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
    
    sha_init(SHA256_MODE);
    ShowProgress(0, 0, path);
    if (DriveType(path) & DRV_VIRTUAL) { // for virtual files
        VirtualFile vfile;
        u32 fsize;
        
        if (!GetVirtualFile(&vfile, path))
            return false;
        fsize = vfile.size;
        
        for (size_t pos = 0; (pos < fsize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, fsize - pos);
            if (ReadVirtualFile(&vfile, MAIN_BUFFER, pos, read_bytes, NULL) != 0)
                ret = false;
            if (!ShowProgress(pos + read_bytes, fsize, path))
                ret = false;
            sha_update(MAIN_BUFFER, read_bytes);
        }
    } else { // for regular FAT files
        FIL file;
        size_t fsize;
        
        if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return false;
        fsize = f_size(&file);
        f_lseek(&file, 0);
        f_sync(&file);
        
        for (size_t pos = 0; (pos < fsize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT bytes_read = 0;
            if (fx_read(&file, MAIN_BUFFER, MAIN_BUFFER_SIZE, &bytes_read) != FR_OK)
                ret = false;
            if (!ShowProgress(pos + bytes_read, fsize, path))
                ret = false;
            sha_update(MAIN_BUFFER, bytes_read);
        }
        fx_close(&file);
    }
    ShowProgress(1, 1, path);
    sha_get(sha256);
    
    return ret;
}

u32 FileFindData(const char* path, u8* data, u32 size, u32 offset) {
    u32 found = (u32) -1;
    u32 fsize = FileGetSize(path);
    
    for (u32 pass = 0; pass < 2; pass++) {
        bool show_progress = false;
        u32 pos = (pass == 0) ? offset : 0;
        u32 search_end = (pass == 0) ? fsize : offset + size;
        search_end = (search_end > fsize) ? fsize : search_end;
        for (; (pos < search_end) && (found == (u32) -1); pos += MAIN_BUFFER_SIZE - (size - 1)) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, search_end - pos);
            if (FileGetData(path, MAIN_BUFFER, read_bytes, pos) != read_bytes)
                break;
            for (u32 i = 0; i + size <= read_bytes; i++) {
                if (memcmp(MAIN_BUFFER + i, data, size) == 0) {
                    found = pos + i;
                    break;
                }
            }
            if (!show_progress && (found == (u32) -1) && (pos + read_bytes < fsize)) {
                ShowProgress(0, 0, path);
                show_progress = true;
            }
            if (show_progress && (!ShowProgress(pos + read_bytes, fsize, path)))
                break;
        }
    }
    
    return found;
}

bool FileInjectFile(const char* dest, const char* orig, u32 offset) {
    VirtualFile dvfile;
    VirtualFile ovfile;
    FIL ofile;
    FIL dfile;
    size_t osize;
    size_t dsize;
    
    bool vdest;
    bool vorig;
    bool ret;
    
    if (!CheckWritePermissions(dest)) return false;
    if (strncmp(dest, orig, 256) == 0) {
        ShowPrompt(false, "Error: Can't inject file into itself");
        return false;
    }
    
    // open destination
    if (DriveType(dest) & DRV_VIRTUAL) {
        vdest = true;
        if (!GetVirtualFile(&dvfile, dest))
            return false;
        dsize = dvfile.size;
    } else {
        vdest = false;
        if (fx_open(&dfile, dest, FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
            return false;
        dsize = f_size(&dfile);
        f_lseek(&dfile, offset);
        f_sync(&dfile);
    }
    
    // open origin
    if (DriveType(orig) & DRV_VIRTUAL) {
        vorig = true;
        if (!GetVirtualFile(&ovfile, orig)) {
            if (!vdest) fx_close(&dfile);
            return false;
        }
        osize = ovfile.size;
    } else {
        vorig = false;
        if (fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            if (!vdest) fx_close(&dfile);
            return false;
        }
        osize = f_size(&ofile);
        f_lseek(&ofile, 0);
        f_sync(&ofile);
    }
    
    // check file limits
    if (offset + osize > dsize) {
        ShowPrompt(false, "Operation would write beyond end of file");
        if (!vdest) fx_close(&dfile);
        if (!vorig) fx_close(&ofile);
        return false;
    }
    
    ret = true;
    ShowProgress(0, 0, orig);
    for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
        UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
        UINT bytes_read = read_bytes;
        UINT bytes_written = read_bytes;
        if ((!vorig && (fx_read(&ofile, MAIN_BUFFER, read_bytes, &bytes_read) != FR_OK)) ||
            (vorig && ReadVirtualFile(&ovfile, MAIN_BUFFER, pos, read_bytes, NULL) != 0))
            ret = false;
        if (!ShowProgress(pos + (bytes_read / 2), osize, orig))
            ret = false;
        if ((!vdest && (fx_write(&dfile, MAIN_BUFFER, read_bytes, &bytes_written) != FR_OK)) ||
            (vdest && WriteVirtualFile(&dvfile, MAIN_BUFFER, offset + pos, read_bytes, NULL) != 0))
            ret = false;
        if (bytes_read != bytes_written)
            ret = false;
    }
    ShowProgress(1, 1, orig);
    
    if (!vdest) fx_close(&dfile);
    if (!vorig) fx_close(&ofile);
    
    return ret;
}

bool PathCopyVirtual(const char* destdir, const char* orig, u32* flags) {
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    char deststr[36 + 1];
    char origstr[36 + 1];
    int ddrvtype = DriveType(destdir);
    int odrvtype = DriveType(orig);
    bool ret = true;
    
    if (oname == NULL) return false; // not a proper origin path
    oname++;
    snprintf(dest, 255, "%s/%s", destdir, oname);
    
    TruncateString(deststr, dest, 36, 8);
    TruncateString(origstr, orig, 36, 8);
    
    if ((ddrvtype & DRV_VIRTUAL) && (odrvtype & DRV_VIRTUAL)) { // virtual to virtual
        VirtualFile dvfile;
        VirtualFile ovfile;
        u32 osize;
        
        if (!GetVirtualFile(&dvfile, dest))
            return false;
        if (!GetVirtualFile(&ovfile, orig))
            return false;
        osize = ovfile.size;
        if (dvfile.size != osize) { // almost impossible, but so what...
            ShowPrompt(false, "Virtual file size mismatch:\n%s\n%s", origstr, deststr);
            return false;
        }
        if (strncmp(dest, orig, 256) == 0) { // destination == origin
            ShowPrompt(false, "Origin equals destination:\n%s\n%s", origstr, deststr);
            return false;
        }
        if ((dvfile.keyslot == ovfile.keyslot) && (dvfile.offset == ovfile.offset)) // this improves copy times
            dvfile.keyslot = ovfile.keyslot = 0xFF;
        
        DeinitExtFS();
        if (!ShowProgress(0, 0, orig)) ret = false;
        for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
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
    } else if (ddrvtype & DRV_VIRTUAL) { // SD card to virtual (other FAT not allowed!)
        VirtualFile dvfile;
        FIL ofile;
        u32 osize;
        
        if (fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return false;
        f_lseek(&ofile, 0);
        f_sync(&ofile);
        osize = f_size(&ofile);
        if (!GetVirtualFile(&dvfile, dest)) {
            VirtualDir vdir;
            if (!GetVirtualDir(&vdir, destdir)) {
                fx_close(&ofile);
                return false;
            } else while (true) {
                if (!ReadVirtualDir(&dvfile, &vdir)) {
                    fx_close(&ofile);
                    return false;
                }
                if (dvfile.size == osize) // search by size should be a last resort solution
                    break; // file found
            }
            snprintf(dest, 255, "%s/%s", destdir, dvfile.name);
            if (!ShowPrompt(true, "Entry not found: %s\nInject into %s instead?", deststr, dest)) {
                fx_close(&ofile);
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
                    fx_close(&ofile);
                    return false;
                }
            } else {
                ShowPrompt(false, "File bigger than available space:\n%s (%s)\n%s (%s)", origstr, osizestr, deststr, dsizestr);
                fx_close(&ofile);
                return false;
            }
        }
        
        DeinitExtFS();
        if (!ShowProgress(0, 0, orig)) ret = false;
        for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
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
    } else if (odrvtype & DRV_VIRTUAL) { // virtual to any file system
        VirtualFile ovfile;
        FIL dfile;
        u32 osize;
        
        if (!GetVirtualFile(&ovfile, orig))
            return false;
        
        // check if destination exists
        if (flags && !(*flags & OVERWRITE_ALL) && fa_stat(dest, NULL) == FR_OK) {
            if (*flags & SKIP_ALL) {
                *flags |= SKIP_CUR;
                return true;
            }
            const char* optionstr[5] =
                {"Choose new name", "Overwrite file", "Skip file", "Overwrite all", "Skip all"};
            u32 user_select = ShowSelectPrompt((*flags & ASK_ALL) ? 5 : 3, optionstr,
                "Destination already exists:\n%s", deststr);
            if (user_select == 1) {
                do {
                    char* dname = strrchr(dest, '/');
                    if (dname == NULL) return false;
                    dname++;
                    if (!ShowStringPrompt(dname, 255 - (dname - dest), "Choose new destination name"))
                        return false;
                } while (fa_stat(dest, NULL) == FR_OK);
            } else if (user_select == 3) {
                *flags |= SKIP_CUR;
                return true;
            } else if (user_select == 4) {
                *flags |= OVERWRITE_ALL;
            } else if (user_select == 5) {
                *flags |= (SKIP_CUR|SKIP_ALL);
                return true;
            } else if (user_select != 2) {
                return false;
            }
        }
        
        if (fx_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
            return false;
        f_lseek(&dfile, 0);
        f_sync(&dfile);
        osize = ovfile.size;
        if (GetFreeSpace(dest) < osize) {
            ShowPrompt(false, "Error: File is too big for destination");
            fx_close(&dfile);
            return false;
        }
        
        if (!ShowProgress(0, 0, orig)) ret = false;
        for (size_t pos = 0; (pos < osize) && ret; pos += MAIN_BUFFER_SIZE) {
            UINT read_bytes = min(MAIN_BUFFER_SIZE, osize - pos);
            UINT bytes_written = 0;
            if (ReadVirtualFile(&ovfile, MAIN_BUFFER, pos, read_bytes, NULL) != 0)
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
    } else {
        return false;
    }
    
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
    
    // the copy process takes place here
    if (!ShowProgress(0, 0, orig)) return false;
    if (move && fa_stat(dest, NULL) != FR_OK) { // moving if dest not existing
        ret = (f_rename(orig, dest) == FR_OK);
    } else if (fno.fattrib & AM_DIR) { // processing folders (same for move & copy)
        DIR pdir;
        char* fname = orig + strnlen(orig, 256);
        
        // create the destination folder if it does not already exist
        if ((fa_opendir(&pdir, dest) != FR_OK) && (f_mkdir(dest) != FR_OK)) {
            ShowPrompt(false, "Error: Overwriting file with dir");
            return false;
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
        size_t fsize;
        
        if (fx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return false;
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
        for (size_t pos = 0; (pos < fsize) && ret; pos += MAIN_BUFFER_SIZE) {
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
    if ((ddrvtype | odrvtype) & DRV_VIRTUAL) {
        // users are inventive...
        if (!(odrvtype & (DRV_SDCARD|DRV_RAMDRIVE|DRV_VIRTUAL)) && (ddrvtype & DRV_VIRTUAL)) {
            ShowPrompt(false, "Only files from SD card or\nramdrive are accepted");
            return false;
        }
        return PathCopyVirtual(destdir, orig, flags);
    } else {
        char fdpath[256]; // 256 is the maximum length of a full path
        char fopath[256];
        strncpy(fdpath, destdir, 255);
        strncpy(fopath, orig, 255);
        bool res = PathCopyWorker(fdpath, fopath, flags, false);
        return res;
    }
}

bool PathMove(const char* destdir, const char* orig, u32* flags) {
    if (!CheckWritePermissions(destdir)) return false;
    if (!CheckWritePermissions(orig)) return false;
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
        bool same_drv = (PathToNumFS(orig) == PathToNumFS(destdir));
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

bool GetRootDirContentsWorker(DirStruct* contents) {
    static const char* drvname[] = {
        "SDCARD",
        "SYSNAND CTRNAND", "SYSNAND TWLN", "SYSNAND TWLP",
        "EMUNAND CTRNAND", "EMUNAND TWLN", "EMUNAND TWLP",
        "IMGNAND CTRNAND", "IMGNAND TWLN", "IMGNAND TWLP",
        "GAME IMAGE",
        "SYSNAND SD", "EMUNAND SD",
        "SYSNAND VIRTUAL", "EMUNAND VIRTUAL", "IMGNAND VIRTUAL",
        "MEMORY VIRTUAL",
        "LAST SEARCH"
    };
    static const char* drvnum[] = {
        "0:", "1:", "2:", "3:", "4:", "5:", "6:", "7:", "8:", "9:", "G:", "A:", "B:", "S:", "E:", "I:", "M:", "Z:"
    };
    u32 n_entries = 0;
    
    // virtual root objects hacked in
    for (u32 pdrv = 0; (pdrv < NORM_FS+VIRT_FS) && (n_entries < MAX_DIR_ENTRIES); pdrv++) {
        DirEntry* entry = &(contents->entry[n_entries]);
        if (!DriveType(drvnum[pdrv])) continue; // drive not available
        memset(entry->path, 0x00, 64);
        snprintf(entry->path + 0,  4, drvnum[pdrv]);
        if ((pdrv == 7) && ((GetMountState() == IMG_FAT) || (GetMountState() == IMG_RAMDRV))) 
            snprintf(entry->path + 4, 32, "[%s] %s", drvnum[pdrv], // FAT image / RAM drive special handling
                (GetMountState() == IMG_FAT) ? "FAT IMAGE" : "RAMDRIVE");
        else if (pdrv == 10) // Game drive special handling
            snprintf(entry->path + 4, 32, "[%s] %s %s", drvnum[pdrv],
                (GetMountState() == GAME_CIA) ? "CIA" :
                (GetMountState() == GAME_NCSD) ? "NCSD" :
                (GetMountState() == GAME_NCCH) ? "NCCH" : "UNK", drvname[pdrv]);
        else snprintf(entry->path + 4, 32, "[%s] %s", drvnum[pdrv], drvname[pdrv]);
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
    
    if (fa_opendir(&pdir, fpath) != FR_OK)
        return false;
    (fname++)[0] = '/';
    
    while (f_readdir(&pdir, &fno) == FR_OK) {
        if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
            continue; // filter out virtual entries
        strncpy(fname, fno.fname, (fnsize - 1) - (fname - fpath));
        if (fno.fname[0] == 0) {
            ret = true;
            break;
        } else if (!pattern || MatchName(pattern, fname)) {
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
            if (contents->n_entries >= MAX_DIR_ENTRIES) {
                ret = true; // Too many entries, still okay
                break;
            }
            contents->n_entries++;
        }
        if (recursive && (fno.fattrib & AM_DIR)) {
            if (!GetDirContentsWorker(contents, fpath, fnsize, pattern, recursive))
                break;
        }
    }
    f_closedir(&pdir);
    
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
        if (DriveType(path) & DRV_VIRTUAL) {
            if (!GetVirtualDirContents(contents, path, pattern))
                contents->n_entries = 0;
        } else {
            char fpath[256]; // 256 is the maximum length of a full path
            strncpy(fpath, path, 256);
            if (!GetDirContentsWorker(contents, fpath, 256, pattern, recursive))
                contents->n_entries = 0;
        }
        SortDirStruct(contents);
    }
}

void GetDirContents(DirStruct* contents, const char* path) {
    if (IsSearchDrive(path)) {
        ShowString("Searching, please wait...");
        SearchDirContents(contents, search_path, search_pattern, true);
        ClearScreenF(true, false, COLOR_STD_BG);
    } else SearchDirContents(contents, path, NULL, false);
}

uint64_t GetFreeSpace(const char* path)
{
    DWORD free_clusters;
    FATFS *fs_ptr;
    char fsname[4] = { '\0' };
    int pdrv = PathToNumFSA(path);
    if (pdrv < 0) return 0;
    
    snprintf(fsname, 3, "%i:", pdrv);
    if (f_getfree(fsname, &free_clusters, &fs_ptr) != FR_OK)
        return 0;

    return (uint64_t) free_clusters * fs[pdrv].csize * _MAX_SS;
}

uint64_t GetTotalSpace(const char* path)
{
    int pdrv = PathToNumFSA(path);
    if (pdrv < 0) return 0;
    
    return (uint64_t) (fs[pdrv].n_fatent - 2) * fs[pdrv].csize * _MAX_SS;
}

uint64_t GetPartitionOffsetSector(const char* path)
{
    int pdrv = PathToNumFSA(path);
    if (pdrv < 0) return -1;
    
    return (uint64_t) fs[pdrv].volbase;
}
