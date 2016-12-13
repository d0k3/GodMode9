#include "image.h"
#include "sddata.h"
#include "ff.h"

static FIL mount_file;
static u32 mount_state = 0;

static char mount_path[256] = { 0 };


int ReadImageBytes(u8* buffer, u64 offset, u64 count) {
    UINT bytes_read;
    UINT ret;
    if (!count) return -1;
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
    return mount_state ? f_sync(&mount_file) : FR_INVALID_OBJECT;
}

u64 GetMountSize(void) {
    return mount_state ? f_size(&mount_file) : 0;
}

u32 GetMountState(void) {
    return mount_state;
}

const char* GetMountPath(void) {
    return mount_path;
}

u32 MountImage(const char* path) {
    u32 type = (path) ? IdentifyFileType(path) : 0;
    if (mount_state) {
        fx_close(&mount_file);
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
