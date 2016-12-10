#include "fsinit.h"
#include "fsdrive.h"
#include "virtual.h"
#include "vgame.h"
#include "sddata.h"
#include "image.h"
#include "ff.h"

// don't use this area for anything else!
static FATFS* fs = (FATFS*)0x20316000;

// currently open file systems
static bool fs_mounted[NORM_FS] = { false };

bool InitSDCardFS() {
    fs_mounted[0] = (f_mount(fs, "0:", 1) == FR_OK);
    return fs_mounted[0];
}

bool InitExtFS() {
    if (!fs_mounted[0])
        return false;
    for (u32 i = 1; i < NORM_FS; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (fs_mounted[i]) continue;
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
    }
    SetupNandSdDrive("A:", "0:", "1:/private/movable.sed", 0);
    SetupNandSdDrive("B:", "0:", "4:/private/movable.sed", 1);
    return true;
}

bool InitImgFS(const char* path) {
    // deinit image filesystem
    for (u32 i = NORM_FS - 1; i >= NORM_FS - IMGN_FS; i--) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (!fs_mounted[i]) continue;
        f_mount(NULL, fsname, 1);
        fs_mounted[i] = false;
    }
    // (re)mount image, done if path == NULL
    MountImage(path);
    InitVGameDrive();
    if (!GetMountState()) return false;
    // reinit image filesystem
    for (u32 i = NORM_FS - IMGN_FS; i < NORM_FS; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
    }
    return true;
}

bool InitRamDriveFS() {
    u32 pdrv = NORM_FS - IMGN_FS;
    char fsname[8];
    snprintf(fsname, 7, "%lu:", pdrv);
    
    InitImgFS(NULL);
    MountRamDrive();
    fs_mounted[pdrv] = (f_mount(fs + pdrv, fsname, 1) == FR_OK);
    if (!fs_mounted[pdrv] && (GetMountState() == IMG_RAMDRV)) {
        f_mkfs(fsname, FM_ANY, 0, MAIN_BUFFER, MAIN_BUFFER_SIZE); // format ramdrive if required
        f_mount(NULL, fsname, 1);
        fs_mounted[pdrv] = (f_mount(fs + pdrv, fsname, 1) == FR_OK);
    }
    
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
        if ((i == NORM_FS - IMGN_FS) && (GetMountState() != IMG_RAMDRV)) { // unmount image
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

void DismountDriveType(u32 type) { // careful with this - no safety checks
    if (type & DriveType(GetMountPath()))
        InitImgFS(NULL); // image is mounted from type -> unmount image drive, too
    for (u32 i = NORM_FS - 1; i > 0; i--) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (!fs_mounted[i] || !(type & DriveType(fsname)))
            continue;
        f_mount(NULL, fsname, 1);
        fs_mounted[i] = false;
    }
}

int GetMountedFSNum(const char* path) {
    char alias[256];
    dealias_path(alias, path);
    int fsnum = *alias - (int) '0';
    if ((fsnum < 0) || (fsnum >= NORM_FS) || (alias[1] != ':') || !fs_mounted[fsnum])
        return -1;
    return fsnum;
}

FATFS* GetMountedFSObject(const char* path) {
    int fsnum = GetMountedFSNum(path);
    return (fsnum >= 0) ? fs + fsnum : NULL;
}
