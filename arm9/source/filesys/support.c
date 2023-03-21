#include "support.h"
#include "fsutil.h" // only for file selector
#include "vram0.h"
#include "vff.h"

#define SUPPORT_FILE_PATHS  "0:/gm9/support", "1:/gm9/support" // we also check the VRAM TAR first
#define SUPPORT_DIR_PATHS   "V:", "0:/gm9", "1:/gm9"


bool CheckSupportFile(const char* fname)
{
    // try VRAM0 first
    if (FindVTarFileInfo(fname, NULL))
        return true;

    // try support file paths
    const char* base_paths[] = { SUPPORT_FILE_PATHS };
    for (u32 i = 0; i < countof(base_paths); i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", base_paths[i], fname);
        if (fvx_stat(path, NULL) == FR_OK)
            return true;
    }

    return false;
}

size_t LoadSupportFile(const char* fname, void* buffer, size_t max_len)
{
    // try VRAM0 first
    u64 len64 = 0;
    void* data = FindVTarFileInfo(fname, &len64);
    if (data && len64 && (len64 < max_len)) {
        memcpy(buffer, data, len64);
        return (size_t) len64;
    }

    // try support file paths
    const char* base_paths[] = { SUPPORT_FILE_PATHS };
    for (u32 i = 0; i < countof(base_paths); i++) {
        UINT len32;
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", base_paths[i], fname);
        if (fvx_qread(path, buffer, 0, max_len, &len32) == FR_OK)
            return len32;
    }

    return 0;
}

bool SaveSupportFile(const char* fname, void* buffer, size_t len)
{
    const char* base_paths[] = { SUPPORT_FILE_PATHS };
    int idx = -1;

    // check for existing support file path
    for (u32 i = 0; (idx < 0) && (i < countof(base_paths)); i++) {
        if (fvx_stat(base_paths[i], NULL) == FR_OK)
            idx = i;
    }

    // create path if required
    for (u32 i = 0; (idx < 0) && (i < countof(base_paths)); i++) {
        if (fvx_rmkdir(base_paths[i]) == FR_OK)
            idx = i;
    }

    // write support file
    if (idx >= 0) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", base_paths[idx], fname);
        fvx_unlink(path);
        if (fvx_qwrite(path, buffer, 0, len, NULL) == FR_OK)
            return true;
    }

    return false;
}

bool SetAsSupportFile(const char* fname, const char* source)
{
    u32 len = fvx_qsize(source);
    if (!len) return false;

    void* buffer = malloc(len);
    if (!buffer) return false;

    bool res = false;
    if (fvx_qread(source, buffer, 0, len, NULL) == FR_OK)
        res = SaveSupportFile(fname, buffer, len);
    free(buffer);

    return res;
}

bool GetSupportDir(char* path, const char* dname)
{
    const char* base_paths[] = { SUPPORT_DIR_PATHS };
    for (u32 i = 0; i < countof(base_paths); i++) {
        FILINFO fno;
        snprintf(path, 256, "%s/%s", base_paths[i], dname);
        if ((fvx_stat(path, &fno) == FR_OK) && (fno.fattrib & AM_DIR))
            return true;
    }

    return false;
}

bool CheckSupportDir(const char* dname)
{
    char path[256];
    return GetSupportDir(path, dname);
}

bool FileSelectorSupport(char* result, const char* text, const char* dname, const char* pattern)
{
    char path[256];
    // result needs to be at least 256 bytes long for this to work!
    if (!GetSupportDir(path, dname)) return false;
    return FileSelector(result, text, path, pattern, HIDE_EXT, false);
}
