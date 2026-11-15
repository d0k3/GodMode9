#include "fsinit.h"
#include "fsdrive.h"
#include "virtual.h"
#include "sddata.h"
#include "image.h"
#include "vcart.h"
#include "ff.h"

// FATFS filesystem objects
static FATFS fs[NORM_FS];

// currently open file systems
static bool fs_mounted[NORM_FS] = { false };

bool InitSDCardFS() {
    fs_mounted[0] = (f_mount(fs, "0:", 1) == FR_OK);
    return fs_mounted[0];
}

bool InitExtFS() {
    static bool ramdrv_ready = false;

    for (u32 i = 1; i < FAT_IMAGE_END; i++) {
        const char* fsname = GetFatDrivePath(i);
        if (fs_mounted[i]) continue;
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
        if ((!fs_mounted[i] || !ramdrv_ready) && (i == FAT_RAM_VOLUME) && !(GetMountState() & IMG_NAND)) {
            u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
            if (!buffer) bkpt; // whatever, this won't go wrong anyways
            f_mkfs(fsname, NULL, buffer, STD_BUFFER_SIZE); // format ramdrive if required
            free(buffer);
            f_mount(NULL, fsname, 1);
            fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
            ramdrv_ready = true;
        }
    }
    SetupNandSdDrive("A:", "0:", "1:/private/movable.sed", 0);
    SetupNandSdDrive("B:", "0:", "4:/private/movable.sed", 1);
    return true;
}

bool InitImgFS(const char* path) {
    if (path && (DriveType(path) & DRV_IMAGE)) return false;
    // find drive # of the last image FAT drive
    u32 drv_i = FAT_IMAGE_FIRST;
    for (; drv_i < FAT_IMAGE_END; drv_i++) {
        const char* fsname = GetFatDrivePath(drv_i);
        if (!(DriveType(fsname)&DRV_IMAGE)) break;
    }
    // deinit virtual filesystem
    DeinitVirtualImageDrive();
    // deinit image filesystem
    DismountDriveType(DRV_IMAGE);
    // (re)mount image, done if path == NULL
    u64 type = MountImage(path);
    InitVirtualImageDrive();
    if ((type&IMG_NAND) && (drv_i < FAT_IMAGE_END)) drv_i = FAT_IMAGE_END;
    else if ((type&IMG_FAT) && (drv_i < FAT_IMAGE_FIRST + 1)) drv_i = FAT_IMAGE_FIRST + 1;
    // reinit image filesystem
    for (u32 i = FAT_IMAGE_FIRST; i < drv_i; i++) {
        const char* fsname = GetFatDrivePath(i);
        fs_mounted[i] = (f_mount(fs + i, fsname, 1) == FR_OK);
    }
    return GetMountState();
}

bool InitFlashcartFS(void) {
    if (fs_mounted[FAT_FLASHCART_VOLUME]) return true;
    if (!GetVCartDSpicoSize()) return false;
    fs_mounted[FAT_FLASHCART_VOLUME] =
        (f_mount(fs + FAT_FLASHCART_VOLUME, FLASHCART_DRIVE, 1) == FR_OK);
    if (!fs_mounted[FAT_FLASHCART_VOLUME])
        f_mount(NULL, FLASHCART_DRIVE, 0);
    return fs_mounted[FAT_FLASHCART_VOLUME];
}

bool DeinitFlashcartFS(void) {
    if ((DriveType(GetMountPath()) & DRV_FLASHCART) && (SyncImage() != FR_OK))
        return false;
    DismountDriveType(DRV_FLASHCART);
    return true;
}

bool IsMountedFlashcart(void) {
    return fs_mounted[FAT_FLASHCART_VOLUME];
}

void DeinitExtFS() {
    InitImgFS(NULL);
    SetupNandSdDrive(NULL, NULL, NULL, 0);
    SetupNandSdDrive(NULL, NULL, NULL, 1);
    for (u32 i = NORM_FS - 1; i > 0; i--) {
        if (fs_mounted[i]) {
            f_mount(NULL, GetFatDrivePath(i), 1);
            fs_mounted[i] = false;
        }
    }
}

void DeinitSDCardFS() {
    DismountDriveType(DRV_SDCARD|DRV_EMUNAND|DRV_ALIAS);
}

void DismountDriveType(u32 type) { // careful with this - no safety checks
    if (type & DriveType(GetMountPath()))
        InitImgFS(NULL); // image is mounted from type -> unmount image drive, too
    if (type & DRV_SDCARD) {
        SetupNandSdDrive(NULL, NULL, NULL, 0);
        SetupNandSdDrive(NULL, NULL, NULL, 1);
    }
    for (u32 i = 0; i < NORM_FS; i++) {
        const char* fsname = GetFatDrivePath(i);
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
    if (!path || !path[0] || (path[1] != ':')) return -1;
    char alias[256];
    dealias_path(alias, path);
    int fsnum = GetFatDriveIndex(alias);
    if ((fsnum < 0) || !fs_mounted[fsnum])
        return -1;
    return fsnum;
}

FATFS* GetMountedFSObject(const char* path) {
    int fsnum = GetMountedFSNum(path);
    return (fsnum >= 0) ? fs + fsnum : NULL;
}
