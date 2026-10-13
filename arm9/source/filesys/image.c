#include "image.h"
#include "vff.h"
#include "nandcmac.h"
#include "vcart.h"

static FIL mount_file;
static u64 mount_state = 0;

static char mount_path[256] = { 0 };

static bool fix_cmac = false;

// DSpico SD card, accessed by sector (can be over 4GB)
static bool dspico_mode = false;


int ReadImageBytes(void* buffer, u64 offset, u64 count) {
    UINT bytes_read;
    UINT ret;
    if (!count) return -1;
    if (!mount_state) return FR_INVALID_OBJECT;
    if (dspico_mode) return ReadVCartDSpicoBytes(buffer, offset, count);
    if (fvx_tell(&mount_file) != offset) {
        if (fvx_size(&mount_file) < offset) return -1;
        fvx_lseek(&mount_file, offset);
    }
    ret = fvx_read(&mount_file, buffer, count, &bytes_read);
    return (ret != 0) ? (int) ret : (bytes_read != count) ? -1 : 0;
}

int WriteImageBytes(const void* buffer, u64 offset, u64 count) {
    UINT bytes_written;
    UINT ret;
    if (!count) return -1;
    if (!mount_state) return FR_INVALID_OBJECT;
    if (dspico_mode) return WriteVCartDSpicoBytes(buffer, offset, count);
    if (fvx_tell(&mount_file) != offset)
        fvx_lseek(&mount_file, offset);
    ret = fvx_write(&mount_file, buffer, count, &bytes_written);
    if (ret == 0) fix_cmac = true;
    return (ret != 0) ? (int) ret : (bytes_written != count) ? -1 : 0;
}

int ReadImageSectors(void* buffer, u32 sector, u32 count) {
    return ReadImageBytes(buffer, (u64) sector * 0x200, (u64) count * 0x200);
}

int WriteImageSectors(const void* buffer, u32 sector, u32 count) {
    return WriteImageBytes(buffer, (u64) sector * 0x200, (u64) count * 0x200);
}

int SyncImage(void) {
    if (dspico_mode) return 0;
    return mount_state ? fvx_sync(&mount_file) : FR_INVALID_OBJECT;
}

bool IsMountedDSpico(void) {
    return dspico_mode;
}

u64 GetMountSize(void) {
    if (dspico_mode) return GetVCartDSpicoSize();
    return mount_state ? fvx_size(&mount_file) : 0;
}

u64 GetMountState(void) {
    return mount_state;
}

const char* GetMountPath(void) {
    return mount_path;
}

u64 MountImage(const char* path) {
    if (mount_state) {
        if (!dspico_mode) {
            fvx_close(&mount_file);
            if (fix_cmac) FixFileCmac(mount_path, false);
        }
        fix_cmac = false;
        dspico_mode = false;
        mount_state = 0;
        *mount_path = 0;
    }
    const char* dspico_path = GetVCartDSpicoImagePath();
    if (path && dspico_path && (strncasecmp(path, dspico_path, 256) == 0)) {
        if (!GetVCartDSpicoSize()) return 0;
        dspico_mode = true;
        strncpy(mount_path, path, 256);
        return (mount_state = IMG_FAT);
    }
    u64 type = (path) ? IdentifyFileType(path) : 0;
    if (!type) return 0;
    if ((fvx_open(&mount_file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) &&
        (fvx_open(&mount_file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK))
        return 0;
    fvx_lseek(&mount_file, 0);
    fvx_sync(&mount_file);
    strncpy(mount_path, path, 256);
    return (mount_state = type);
}
