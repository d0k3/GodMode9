#include "fsinit.h"
#include "fsdrive.h"
#include "virtual.h"
#include "sddata.h"
#include "image.h"
#include "ff.h"

// FATFS filesystem objects (x10)
static FATFS fs[NORM_FS];

// currently open file systems
static bool fs_mounted[NORM_FS] = { false };

bool InitSDCardFS() {
    fs_mounted[0] = (f_mount(fs, "0:", 1) == FR_OK);
    return fs_mounted[0];
}

bool InitExtFS() {
    for (u32 i = 1; i < NORM_FS; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (fs_mounted[i]) continue;
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
        if (!fs_mounted[i] && (i == NORM_FS - 1) && !(GetMountState() & IMG_NAND)) {
            f_mkfs(fsname, FM_ANY, 0, MAIN_BUFFER, MAIN_BUFFER_SIZE); // format ramdrive if required
            f_mount(NULL, fsname, 1);
            fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
        }
    }
    SetupNandSdDrive("A:", "0:", "1:/private/movable.sed", 0);
    SetupNandSdDrive("B:", "0:", "4:/private/movable.sed", 1);
    return true;
}

bool InitImgFS(const char* path) {
    // find drive # of the last image FAT drive
    u32 drv_i = NORM_FS - IMGN_FS;
    char fsname[8];
    for (; drv_i < NORM_FS; drv_i++) {
        snprintf(fsname, 7, "%lu:", drv_i);
        if (!(DriveType(fsname)&DRV_IMAGE)) break;
    }
    // deinit image filesystem
    DismountDriveType(DRV_IMAGE);
    // (re)mount image, done if path == NULL
    u32 type = MountImage(path);
    InitVirtualImageDrive();
    if ((type&IMG_NAND) && (drv_i < NORM_FS)) drv_i = NORM_FS;
    else if ((type&IMG_FAT) && (drv_i < NORM_FS - IMGN_FS + 1)) drv_i = NORM_FS - IMGN_FS + 1;
    // reinit image filesystem
    for (u32 i = NORM_FS - IMGN_FS; i < drv_i; i++) {
        snprintf(fsname, 7, "%lu:", i);
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
    }
    return GetMountState();
}

void DeinitExtFS() {
    SetupNandSdDrive(NULL, NULL, NULL, 0);
    SetupNandSdDrive(NULL, NULL, NULL, 1);
    InitImgFS(NULL);
    for (u32 i = NORM_FS - 1; i > 0; i--) {
        if (fs_mounted[i]) {
            char fsname[8];
            snprintf(fsname, 7, "%lu:", i);
            f_mount(NULL, fsname, 1);
            fs_mounted[i] = false;
        }
    }
}

void DeinitSDCardFS() {
    DismountDriveType(DRV_SDCARD|DRV_EMUNAND);
}

void DismountDriveType(u32 type) { // careful with this - no safety checks
    if (type & DriveType(GetMountPath()))
        InitImgFS(NULL); // image is mounted from type -> unmount image drive, too
    if (type & DRV_SDCARD) {
        SetupNandSdDrive(NULL, NULL, NULL, 0);
        SetupNandSdDrive(NULL, NULL, NULL, 1);
    }
    for (u32 i = 0; i < NORM_FS; i++) {
        char fsname[8];
        snprintf(fsname, 7, "%lu:", i);
        if (!fs_mounted[i] || !(type & DriveType(fsname)))
            continue;
        f_mount(NULL, fsname, 1);
        fs_mounted[i] = false;
    }
}

bool CheckSDMountState(void) {
    return fs_mounted[0] || fs_mounted[4] || fs_mounted[5] || fs_mounted[6];
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
