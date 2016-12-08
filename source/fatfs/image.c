#include "image.h"
#include "sddata.h"
#include "platform.h"
#include "ff.h"

static u8* ramdrv_buffer = NULL;
static u32 ramdrv_size = 0;

static FIL mount_file;
static u32 mount_state = 0;

static char mount_path[256] = { 0 };


int ReadImageBytes(u8* buffer, u64 offset, u64 count) {
    UINT bytes_read;
    UINT ret;
    if (!count) return -1;
    if (mount_state == IMG_RAMDRV) {
        if ((offset + count) > ramdrv_size) return -1;
        memcpy(buffer, ramdrv_buffer + (offset), count);
        return 0;
    }
    if (!mount_state) return FR_INVALID_OBJECT;
    if (f_tell(&mount_file) != offset) {
        if (f_size(&mount_file) < offset) return -1;
        f_lseek(&mount_file, offset); 
    }
    ret = fx_read(&mount_file, buffer, count, &bytes_read);
    return (ret != 0) ? (int) ret : (bytes_read != count) ? -1 : 0;
}

int WriteImageBytes(const u8* buffer, u64 offset, u64 count) {
    UINT bytes_written;
    UINT ret;
    if (!count) return -1;
    if (mount_state == IMG_RAMDRV) {
        if ((offset + count) > ramdrv_size) return -1;
        memcpy(ramdrv_buffer + (offset), buffer, count);
        return 0;
    }
    if (!mount_state) return FR_INVALID_OBJECT;
    if (f_tell(&mount_file) != offset)
        f_lseek(&mount_file, offset);
    ret = fx_write(&mount_file, buffer, count, &bytes_written);
    return (ret != 0) ? (int) ret : (bytes_written != count) ? -1 : 0;
}

int ReadImageSectors(u8* buffer, u32 sector, u32 count) {
    return ReadImageBytes(buffer, sector * 0x200, count * 0x200);
}

int WriteImageSectors(const u8* buffer, u32 sector, u32 count) {
    return WriteImageBytes(buffer, sector * 0x200, count * 0x200);
}

int SyncImage(void) {
    return (mount_state == IMG_RAMDRV) ? FR_OK :
        mount_state ? f_sync(&mount_file) : FR_INVALID_OBJECT;
}

u64 GetMountSize(void) {
    return (mount_state == IMG_RAMDRV) ? ramdrv_size :
        mount_state ? f_size(&mount_file) : 0;
}

u32 GetMountState(void) {
    return mount_state;
}

const char* GetMountPath(void) {
    return mount_path;
}

u32 MountRamDrive(void) {
    if (mount_state && (mount_state != IMG_RAMDRV))
        fx_close(&mount_file);
    if (GetUnitPlatform() == PLATFORM_3DS) {
        ramdrv_buffer = RAMDRV_BUFFER_O3DS;
        ramdrv_size = RAMDRV_SIZE_O3DS;
    } else {
        ramdrv_buffer = RAMDRV_BUFFER_N3DS;
        ramdrv_size = RAMDRV_SIZE_N3DS;
    }
    *mount_path = 0;
    return (mount_state = IMG_RAMDRV);
}

u32 MountImage(const char* path) {
    u32 type = (path) ? IdentifyFileType(path) : 0;
    if (mount_state) {
        if (mount_state != IMG_RAMDRV) fx_close(&mount_file);
        mount_state = 0;
        *mount_path = 0;
    }
    if (!type) return 0;
    if ((fx_open(&mount_file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) &&
        (fx_open(&mount_file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK))
        return 0;
    f_lseek(&mount_file, 0);
    f_sync(&mount_file);
    strncpy(mount_path, path, 255);
    return (mount_state = type);
}
