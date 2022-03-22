#include "fsutil.h"
#include "fsinit.h"
#include "fsdrive.h"
#include "fsperm.h"
#include "sddata.h"
#include "vff.h"
#include "virtual.h"
#include "image.h"
#include "sha.h"
#include "sdmmc.h"
#include "ff.h"
#include "ui.h"
#include "swkbd.h"
#include "language.h"

#define SKIP_CUR        (1UL<<11)
#define OVERWRITE_CUR   (1UL<<12)

#define _MAX_FS_OPT     8 // max file selector options

// Volume2Partition resolution table
PARTITION VolToPart[] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
    {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}
};

uint64_t GetSDCardSize() {
    if (sdmmc_sdcard_init() != 0) return 0;
    return (u64) getMMCDevice(1)->total_size * 512;
}

bool FormatSDCard(u64 hidden_mb, u32 cluster_size, const char* label) {
    u8 mbr[0x200] = { 0 };
    u8 ncsd[0x200] = { 0 };
    u8 mbrdata[0x42] = {
        0x80, 0x01, 0x01, 0x00, 0x0C, 0xFE, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0x01, 0x01, 0x00, 0xDA, 0xFE, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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
        ShowPrompt(false, "%s", STR_ERROR_SD_TOO_SMALL);
        return false;
    }

    // Write protection check
    if (SD_WRITE_PROTECTED) {
        ShowPrompt(false, "%s", STR_SD_WRITE_PROTECTED_CANT_CONTINUE);
        return false;
    }

    // build the MBR
    memcpy(mbrdata + 0x08, &fat_sector, 4);
    memcpy(mbrdata + 0x0C, &fat_size, 4);
    memcpy(mbrdata + 0x18, &emu_sector, 4);
    memcpy(mbrdata + 0x1C, &emu_size, 4);
    memcpy(mbr + 0x1BE, mbrdata, 0x42);
    if (hidden_mb) memcpy(mbr, "GATEWAYNAND", 12); // legacy
    else memset(mbr + 0x1CE, 0, 0x10);

    // one last warning....
    // 0:/Nintendo 3DS/ write permission is ignored here, this warning is enough
    if (!ShowUnlockSequence(5, "%s", STR_WARNING_PROCEEDING_WILL_FORMAT_SD_DELETE_ALL_DATA))
        return false;
    ShowString("%s", STR_FORMATTING_SD_PLEASE_WAIT);

    // write the MBR to disk
    // !this assumes a fully deinitialized file system!
    if ((sdmmc_sdcard_init() != 0) || (sdmmc_sdcard_writesectors(0, 1, mbr) != 0) ||
        (emu_size && ((sdmmc_nand_readsectors(0, 1, ncsd) != 0) || (sdmmc_sdcard_writesectors(1, 1, ncsd) != 0)))) {
        ShowPrompt(false, "%s", STR_ERROR_SD_CARD_IO_FAILURE);
        return false;
    }

    // format the SD card
    VolToPart[0].pt = 1; // workaround to prevent FatFS rebuilding the MBR
    InitSDCardFS();

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) bkpt; // will not happen
    MKFS_PARM opt0, opt1;
    opt0.fmt = opt1.fmt = FM_FAT32;
    opt0.au_size = cluster_size;
    opt1.au_size = 0;
    opt0.align = opt1.align = 0;
    opt0.n_fat = opt1.n_fat = 1;
    opt0.n_root = opt1.n_root = 0;
    bool ret = ((f_mkfs("0:", &opt0, buffer, STD_BUFFER_SIZE) == FR_OK) ||
        (f_mkfs("0:", &opt1, buffer, STD_BUFFER_SIZE) == FR_OK)) &&
        (f_setlabel((label) ? label : "0:GM9SD") == FR_OK);
    free(buffer);

    DeinitSDCardFS();
    VolToPart[0].pt = 0; // revert workaround to prevent SD mount problems

    return ret;
}

bool SetupBonusDrive(void) {
    if (!ShowUnlockSequence(3, "%s", STR_FORMAT_BONUS_DRIVE_DELETE_ALL_DATA))
        return false;
    ShowString("%s", STR_FORMATTING_DRIVE_PLEASE_WAIT);
    if (GetMountState() & IMG_NAND) InitImgFS(NULL);

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) bkpt;
    bool ret = (f_mkfs("8:", NULL, buffer, STD_BUFFER_SIZE) == FR_OK);
    free(buffer);

    if (ret) {
        f_setlabel("8:BONUS");
        InitExtFS();
    }
    return ret;
}

bool FileUnlock(const char* path) {
    FIL file;
    FRESULT res;

    if (!(DriveType(path) & DRV_FAT)) return true; // can't really check this
    if ((res = fx_open(&file, path, FA_READ | FA_OPEN_EXISTING)) != FR_OK) {
        char pathstr[UTF_BUFFER_BYTESIZE(32)];
        TruncateString(pathstr, path, 32, 8);
        if (GetMountState() && (res == FR_LOCKED) &&
            (ShowPrompt(true, "%s\n%s", pathstr, STR_FILE_IS_MOUNTED_UNMOUNT_TO_UNLOCK))) {
            InitImgFS(NULL);
            if (fx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
                return false;
        } else return false;
    }
    fx_close(&file);

    return true;
}

bool FileSetData(const char* path, const void* data, size_t size, size_t foffset, bool create) {
    UINT bw;
    if (!CheckWritePermissions(path)) return false;
    if ((DriveType(path) & DRV_FAT) && create) f_unlink(path);
    return (fvx_qwrite(path, data, foffset, size, &bw) == FR_OK) && (bw == size);
}

size_t FileGetData(const char* path, void* data, size_t size, size_t foffset) {
    UINT br;
    if (fvx_qread(path, data, foffset, size, &br) != FR_OK) br = 0;
    return br;
}

size_t FileGetSize(const char* path) {
    FILINFO fno;
    if (fvx_stat(path, &fno) != FR_OK)
        return 0;
    return fno.fsize;
}

bool FileGetSha(const char* path, u8* hash, u64 offset, u64 size, bool sha1) {
    bool ret = true;
    FIL file;
    u64 fsize;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;

    fsize = fvx_size(&file);
    if (offset + size > fsize) return false;
    if (!size) size = fsize - offset;
    fvx_lseek(&file, offset);

    u32 bufsiz = min(STD_BUFFER_SIZE, fsize);
    u8* buffer = (u8*) malloc(bufsiz);
    if (!buffer) return false;

    ShowProgress(0, 0, path);
    sha_init(sha1 ? SHA1_MODE : SHA256_MODE);
    for (u64 pos = 0; (pos < size) && ret; pos += bufsiz) {
        UINT read_bytes = min(bufsiz, size - pos);
        UINT bytes_read = 0;
        if (fvx_read(&file, buffer, read_bytes, &bytes_read) != FR_OK)
            ret = false;
        if (!ShowProgress(pos + bytes_read, size, path))
            ret = false;
        sha_update(buffer, bytes_read);
    }

    sha_get(hash);
    fvx_close(&file);
    free(buffer);

    ShowProgress(1, 1, path);

    return ret;
}

u32 FileFindData(const char* path, u8* data, u32 size_data, u32 offset_file) {
    FIL file; // used for FAT & virtual
    u64 found = (u64) -1;
    u64 fsize = FileGetSize(path);

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return found;

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) return false;

    // main routine
    for (u32 pass = 0; pass < 2; pass++) {
        bool show_progress = false;
        u64 pos = (pass == 0) ? offset_file : 0;
        u64 search_end = (pass == 0) ? fsize : offset_file + size_data;
        search_end = (search_end > fsize) ? fsize : search_end;
        for (; (pos < search_end) && (found == (u64) -1); pos += STD_BUFFER_SIZE - (size_data - 1)) {
            UINT read_bytes = min(STD_BUFFER_SIZE, search_end - pos);
            UINT btr;
            fvx_lseek(&file, pos);
            if ((fvx_read(&file, buffer, read_bytes, &btr) != FR_OK) || (btr != read_bytes))
                break;
            for (u32 i = 0; i + size_data <= read_bytes; i++) {
                if (memcmp(buffer + i, data, size_data) == 0) {
                    found = pos + i;
                    break;
                }
            }
            if (!show_progress && (found == (u64) -1) && (pos + read_bytes < fsize)) {
                ShowProgress(0, 0, path);
                show_progress = true;
            }
            if (show_progress && (!ShowProgress(pos + read_bytes, fsize, path)))
                break;
        }
    }

    free(buffer);
    fvx_close(&file);

    return found;
}

bool FileInjectFile(const char* dest, const char* orig, u64 off_dest, u64 off_orig, u64 size, u32* flags) {
    FIL ofile;
    FIL dfile;
    bool allow_expand = (flags && (*flags & ALLOW_EXPAND));

    if (!CheckWritePermissions(dest)) return false;
    if (strncasecmp(dest, orig, 256) == 0) {
        ShowPrompt(false, "%s", STR_ERROR_CANT_INJECT_FILE_INTO_ITSELF);
        return false;
    }

    // open destination / origin
    if (fvx_open(&dfile, dest, FA_WRITE | ((allow_expand) ? FA_OPEN_ALWAYS : FA_OPEN_EXISTING)) != FR_OK)
        return false;
    if ((fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) &&
        (!FileUnlock(orig) || (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))) {
        fvx_close(&dfile);
        return false;
    }
    fvx_lseek(&dfile, off_dest);
    fvx_lseek(&ofile, off_orig);
    if (!size && (off_orig < fvx_size(&ofile)))
        size = fvx_size(&ofile) - off_orig;

    // check file limits
    if (!allow_expand && (off_dest + size > fvx_size(&dfile))) {
        ShowPrompt(false, "%s", STR_OPERATION_WOULD_WRITE_BEYOND_EOF);
        fvx_close(&dfile);
        fvx_close(&ofile);
        return false;
    } else if (off_orig + size > fvx_size(&ofile)) {
        ShowPrompt(false, "%s", STR_NOT_ENOUGH_DATA_IN_FILE);
        fvx_close(&dfile);
        fvx_close(&ofile);
        return false;
    }

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) return false;

    bool ret = true;
    ShowProgress(0, 0, orig);
    for (u64 pos = 0; (pos < size) && ret; pos += STD_BUFFER_SIZE) {
        UINT read_bytes = min(STD_BUFFER_SIZE, size - pos);
        UINT bytes_read = read_bytes;
        UINT bytes_written = read_bytes;
        if ((fvx_read(&ofile, buffer, read_bytes, &bytes_read) != FR_OK) ||
            (fvx_write(&dfile, buffer, read_bytes, &bytes_written) != FR_OK) ||
            (bytes_read != bytes_written))
            ret = false;
        if (ret && !ShowProgress(pos + bytes_read, size, orig)) {
            if (flags && (*flags & NO_CANCEL)) {
                ShowPrompt(false, "%s", STR_CANCEL_IS_NOT_ALLOWED_HERE);
            } else ret = !ShowPrompt(true, "%s", STR_B_DETECTED_CANCEL);
            ShowProgress(0, 0, orig);
            ShowProgress(pos + bytes_read, size, orig);
        }
    }
    ShowProgress(1, 1, orig);

    free(buffer);
    fvx_close(&dfile);
    fvx_close(&ofile);

    return ret;
}

bool FileSetByte(const char* dest, u64 offset, u64 size, u8 fillbyte, u32* flags) {
    FIL dfile;
    bool allow_expand = (flags && (*flags & ALLOW_EXPAND));

    if (!CheckWritePermissions(dest)) return false;

    // open destination
    if (fvx_open(&dfile, dest, FA_WRITE | ((allow_expand) ? FA_OPEN_ALWAYS : FA_OPEN_EXISTING)) != FR_OK)
        return false;
    fvx_lseek(&dfile, offset);
    if (!size && (offset < fvx_size(&dfile)))
        size = fvx_size(&dfile) - offset;

    // check file limits
    if (!allow_expand && (offset + size > fvx_size(&dfile))) {
        ShowPrompt(false, "%s", STR_OPERATION_WOULD_WRITE_BEYOND_EOF);
        fvx_close(&dfile);
        return false;
    }

    u32 bufsiz = min(STD_BUFFER_SIZE, size);
    u8* buffer = (u8*) malloc(bufsiz);
    if (!buffer) return false;
    memset(buffer, fillbyte, bufsiz);

    bool ret = true;
    ShowProgress(0, 0, dest);
    for (u64 pos = 0; (pos < size) && ret; pos += bufsiz) {
        UINT write_bytes = min(bufsiz, size - pos);
        UINT bytes_written = write_bytes;
        if ((fvx_write(&dfile, buffer, write_bytes, &bytes_written) != FR_OK) ||
            (write_bytes != bytes_written))
            ret = false;
        if (ret && !ShowProgress(pos + bytes_written, size, dest)) {
            if (flags && (*flags & NO_CANCEL)) {
                ShowPrompt(false, "%s", STR_CANCEL_IS_NOT_ALLOWED_HERE);
            } else ret = !ShowPrompt(true, "%s", STR_B_DETECTED_CANCEL);
            ShowProgress(0, 0, dest);
            ShowProgress(pos + bytes_written, size, dest);
        }
    }
    ShowProgress(1, 1, dest);

    free(buffer);
    fvx_close(&dfile);

    return ret;
}

bool FileCreateDummy(const char* cpath, const char* filename, u64 size) {
    char npath[256]; // 256 is the maximum length of a full path
    if (!CheckWritePermissions(cpath)) return false;
    if (filename) snprintf(npath, 255, "%s/%s", cpath, filename);
    else snprintf(npath, 255, "%s", cpath);

    // create dummy file (fail if already existing)
    // then, expand the file size via cluster preallocation
    FIL dfile;
    if (fx_open(&dfile, npath, FA_WRITE | FA_CREATE_NEW) != FR_OK)
        return false;
    f_lseek(&dfile, size > 0xFFFFFFFF ? 0xFFFFFFFF : (FSIZE_t) size);
    f_sync(&dfile);
    fx_close(&dfile);

    return (fa_stat(npath, NULL) == FR_OK);
}

bool DirCreate(const char* cpath, const char* dirname) {
    char npath[256]; // 256 is the maximum length of a full path
    if (!CheckWritePermissions(cpath)) return false;
    snprintf(npath, 255, "%s/%s", cpath, dirname);
    if (fa_mkdir(npath) != FR_OK) return false;
    return (fa_stat(npath, NULL) == FR_OK);
}

bool DirInfoWorker(char* fpath, bool virtual, u64* tsize, u32* tdirs, u32* tfiles) {
    char* fname = fpath + strnlen(fpath, 256 - 1);
    bool ret = true;
    if (virtual) {
        VirtualDir vdir;
        VirtualFile vfile;
        if (!GetVirtualDir(&vdir, fpath)) return false; // get dir reader object
        while (ReadVirtualDir(&vfile, &vdir)) {
            if (vfile.flags & VFLAG_DIR) {
                (*tdirs)++;
                *(fname++) = '/';
                GetVirtualFilename(fname, &vfile, (256 - 1) - (fname - fpath));
                if (!DirInfoWorker(fpath, virtual, tsize, tdirs, tfiles)) ret = false;
                *(--fname) = '\0';
            } else {
                *tsize += vfile.size;
                (*tfiles)++;
            }
        }
    } else {
        DIR pdir;
        FILINFO fno;
        if (fa_opendir(&pdir, fpath) != FR_OK) return false; // get dir reader object
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            if (fno.fname[0] == 0) break; // end of dir
            if (fno.fattrib & AM_DIR) {
                (*tdirs)++;
                *(fname++) = '/';
                strncpy(fname, fno.fname, (256 - 1) - (fname - fpath));
                if (!DirInfoWorker(fpath, virtual, tsize, tdirs, tfiles)) ret = false;
                *(--fname) = '\0';
            } else {
                *tsize += fno.fsize;
                (*tfiles)++;
            }
        }
        f_closedir(&pdir);
    }

    return ret;
}

bool DirInfo(const char* path, u64* tsize, u32* tdirs, u32* tfiles) {
    bool virtual = (DriveType(path) & DRV_VIRTUAL);
    char fpath[256];
    strncpy(fpath, path, 256);
    fpath[255] = '\0';
    *tsize = *tdirs = *tfiles = 0;
    bool res = DirInfoWorker(fpath, virtual, tsize, tdirs, tfiles);
    return res;
}

bool PathExist(const char* path) {
    return (fvx_stat(path, NULL) == FR_OK);
}

bool PathMoveCopyRec(char* dest, char* orig, u32* flags, bool move, u8* buffer, u32 bufsiz) {
    bool to_virtual = GetVirtualSource(dest);
    bool silent = (flags && (*flags & SILENT));
    bool append = (flags && (*flags & APPEND_ALL));
    bool calcsha = (flags && (*flags & CALC_SHA) && !append);
    bool sha1 = (flags && (*flags & USE_SHA1));
    bool ret = false;

    // check destination write permission (special paths only)
    if (((*dest == '1') || (strncmp(dest, "0:/Nintendo 3DS", 16) == 0)) &&
        (!flags || !(*flags & OVERRIDE_PERM)) &&
        !CheckWritePermissions(dest)) return false;

    FILINFO fno;
    if (fvx_stat(orig, &fno) != FR_OK) return false; // origin does not exist
    if (move && (to_virtual || fno.fattrib & AM_VRT)) return false; // trying to move a virtual file

    // path string (for output)
    char deststr[UTF_BUFFER_BYTESIZE(36)];
    TruncateString(deststr, dest, 36, 8);

    // the copy process takes place here
    if (!ShowProgress(0, 0, orig) && !(flags && (*flags & NO_CANCEL))) {
        if (ShowPrompt(true, "%s\n%s", deststr, STR_B_DETECTED_CANCEL)) return false;
        ShowProgress(0, 0, orig);
    }
    if (move && fvx_stat(dest, NULL) != FR_OK) { // moving if dest not existing
        ret = (fvx_rename(orig, dest) == FR_OK);
    } else if (fno.fattrib & AM_DIR) { // processing folders (same for move & copy)
        DIR pdir;
        char* fname = orig + strnlen(orig, 256);

        if (append) {
            if (!silent) ShowPrompt(false, "%s\n%s", deststr, STR_ERROR_CANNOT_APPEND_FOLDER);
            return false;
        }

        // create the destination folder if it does not already exist
        if (fvx_opendir(&pdir, dest) != FR_OK) {
            if (fvx_mkdir(dest) != FR_OK) {
                if (!silent) ShowPrompt(false, "%s\n%s", deststr, STR_ERROR_OVERWRITING_FILE_WITH_DIR);
                return false;
            }
        } else fvx_closedir(&pdir);

        if (fvx_opendir(&pdir, orig) != FR_OK)
            return false;
        *(fname++) = '/';

        while (fvx_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, 256 - (fname - orig));
            if (fno.fname[0] == 0) {
                ret = true;
                break;
            } else {
                // recursively process directory
                char* oname = strrchr(orig, '/');
                char* dname = dest + strnlen(dest, 255);
                if (oname == NULL) return false; // not a proper origin path
                strncpy(dname, oname, 256 - (dname - dest)); // copy name plus preceding '/'
                bool res = PathMoveCopyRec(dest, orig, flags, move, buffer, bufsiz);
                *dname = '\0';
                if (!res) break;
            }
        }

        fvx_closedir(&pdir);
        *(--fname) = '\0';
    } else if (move) { // moving if destination exists
        if (fvx_stat(dest, &fno) != FR_OK) return false;
        if (fno.fattrib & AM_DIR) {
            if (!silent) ShowPrompt(false, "%s\n%s", deststr, STR_ERROR_OVERWRITING_DIR_WITH_FILE);
            return false;
        }
        if (fvx_unlink(dest) != FR_OK) return false;
        ret = (fvx_rename(orig, dest) == FR_OK);
    } else { // copying files
        FIL ofile;
        FIL dfile;
        u64 osize;
        u64 dsize;

        if (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            if (!FileUnlock(orig) || (fvx_open(&ofile, orig, FA_READ | FA_OPEN_EXISTING) != FR_OK))
                return false;
            ShowProgress(0, 0, orig); // reinit progress bar
        }

        if ((!append || (fvx_open(&dfile, dest, FA_WRITE | FA_OPEN_EXISTING) != FR_OK)) &&
            (fvx_open(&dfile, dest, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)) {
            if (!silent) ShowPrompt(false, "%s\n%s", deststr, STR_ERROR_CANNOT_OPEN_DESTINATION_FILE);
            fvx_close(&ofile);
            return false;
        }

        ret = true; // destination file exists by now, so we need to handle deletion
        osize = fvx_size(&ofile);
        dsize = append ? fvx_size(&dfile) : 0; // always 0 if not appending to file
        if ((fvx_lseek(&dfile, (osize + dsize)) != FR_OK) || (fvx_sync(&dfile) != FR_OK) || (fvx_tell(&dfile) != (osize + dsize))) { // check space via cluster preallocation
            if (!silent) ShowPrompt(false, "%s\n%s", deststr, STR_ERROR_NOT_ENOUGH_SPACE_AVAILABLE);
            ret = false;
        }

        fvx_lseek(&dfile, dsize);
        fvx_sync(&dfile);
        fvx_lseek(&ofile, 0);
        fvx_sync(&ofile);

        if (calcsha) sha_init(sha1 ? SHA1_MODE : SHA256_MODE);
        for (u64 pos = 0; (pos < osize) && ret; pos += bufsiz) {
            UINT bytes_read = 0;
            UINT bytes_written = 0;
            if ((fvx_read(&ofile, buffer, bufsiz, &bytes_read) != FR_OK) ||
                (fvx_write(&dfile, buffer, bytes_read, &bytes_written) != FR_OK) ||
                (bytes_read != bytes_written))
                ret = false;

            u64 current = pos + bytes_read;
            u64 total = osize;
            if (ret && !ShowProgress(current, total, orig)) {
                if (flags && (*flags & NO_CANCEL)) {
                    ShowPrompt(false, "%s\n%s", deststr, STR_CANCEL_IS_NOT_ALLOWED_HERE);
                } else ret = !ShowPrompt(true, "%s\n%s", deststr, STR_B_DETECTED_CANCEL);
                ShowProgress(0, 0, orig);
                ShowProgress(current, total, orig);
            }
            if (calcsha)
                sha_update(buffer, bytes_read);
        }
        ShowProgress(1, 1, orig);

        fvx_close(&ofile);
        fvx_close(&dfile);
        if (!ret && ((dsize == 0) || (fvx_lseek(&dfile, dsize) != FR_OK) || (f_truncate(&dfile) != FR_OK))) {
            fvx_unlink(dest);
        } else if (!to_virtual && calcsha) {
            u8 hash[0x20];
            char* ext_sha = dest + strnlen(dest, 256);
            snprintf(ext_sha, 256 - (ext_sha - dest), ".sha%c", sha1 ? '1' : '\0');
            sha_get(hash);
            FileSetData(dest, hash, sha1 ? 20 : 32, 0, true);
        }
    }

    return ret;
}

bool PathMoveCopy(const char* dest, const char* orig, u32* flags, bool move) {
    // check permissions
    if (!flags || !(*flags & OVERRIDE_PERM)) {
        if (!CheckWritePermissions(dest)) return false;
        if (move && !CheckDirWritePermissions(orig)) return false;
    }

    // reset local flags
    if (flags) *flags = *flags & ~(SKIP_CUR|OVERWRITE_CUR);

    // preparations
    int ddrvtype = DriveType(dest);
    int odrvtype = DriveType(orig);
    char ldest[256]; // 256 is the maximum length of a full path
    char lorig[256];
    strncpy(ldest, dest, 256);
    strncpy(lorig, orig, 256);
    char deststr[UTF_BUFFER_BYTESIZE(36)];
    TruncateString(deststr, ldest, 36, 8);

    // moving only for regular FAT drives (= not alias drives)
    if (move && !(ddrvtype & odrvtype & DRV_STDFAT)) {
        ShowPrompt(false, "%s", STR_ERROR_ONLY_FAT_FILES_CAN_BE_MOVED);
        return false;
    }

    // is destination part of origin?
    u32 olen = strnlen(lorig, 255);
    if ((strncasecmp(ldest, lorig, olen) == 0) && (ldest[olen] == '/')) {
        ShowPrompt(false, "%s\n%s", deststr, STR_ERROR_DESTINATION_IS_PART_OF_ORIGIN);
        return false;
    }

    if (!(ddrvtype & DRV_VIRTUAL)) { // FAT destination handling
        // get destination name
        char* dname = strrchr(ldest, '/');
        if (!dname) return false;
        dname++;

        // check & fix destination == origin
        while (strncasecmp(ldest, lorig, 255) == 0) {
            if (!ShowKeyboardOrPrompt(dname, 255 - (dname - ldest), "%s\n%s", deststr, STR_ERROR_DESTINATION_EQUALS_ORIGIN_CHOOSE_ANOTHER_NAME))
                return false;
        }

        // check if destination exists
        if (flags && !(*flags & (OVERWRITE_CUR|OVERWRITE_ALL|APPEND_ALL)) && (fa_stat(ldest, NULL) == FR_OK)) {
            if (*flags & SKIP_ALL) {
                *flags |= SKIP_CUR;
                return true;
            }
            const char* optionstr[5] =
                {STR_CHOOSE_NEW_NAME, STR_OVERWRITE_FILES, STR_SKIP_FILES, STR_OVERWRITE_ALL, STR_SKIP_ALL};
            u32 user_select = ShowSelectPrompt((*flags & ASK_ALL) ? 5 : 3, optionstr, STR_DESTINATION_ALREADY_EXISTS, deststr);
            if (user_select == 1) {
                do {
                    if (!ShowKeyboardOrPrompt(dname, 255 - (dname - ldest), "%s", STR_CHOOSE_NEW_DESTINATION_NAME))
                        return false;
                } while (fa_stat(ldest, NULL) == FR_OK);
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

        // ensure the destination path exists
        if (flags && (*flags & BUILD_PATH)) fvx_rmkpath(ldest);

        // setup buffer
        u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
        if (!buffer) {
            ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
            return false;
        }

        // actual move / copy operation
        bool same_drv = (strncasecmp(lorig, ldest, 2) == 0);
        bool res = PathMoveCopyRec(ldest, lorig, flags, move && same_drv, buffer, STD_BUFFER_SIZE);
        if (move && res && (!flags || !(*flags&SKIP_CUR))) PathDelete(lorig);

        free(buffer);
        return res;
    } else { // virtual destination handling
        // can't write an SHA file to a virtual destination
        if (flags) *flags &= ~CALC_SHA;
        bool force_unmount = false;

        // handle NAND image unmounts
        if ((ddrvtype & (DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE)) && !(GetVirtualSource(dest) & (VRT_DISADIFF | VRT_BDRI))) {
            FILINFO fno;
            // virtual NAND files over 4 MB require unmount, totally arbitrary limit (hacky!)
            if ((fvx_stat(ldest, &fno) == FR_OK) && (fno.fsize > 4 * 1024 * 1024))
                force_unmount = true;
        }

        // prevent illegal operations
        if (force_unmount && (odrvtype & ddrvtype & (DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE))) {
            ShowPrompt(false, "%s", STR_COPY_OPERATION_IS_NOT_ALLOWED);
            return false;
        }

        // check destination == origin
        if (strncasecmp(ldest, lorig, 255) == 0) {
            ShowPrompt(false, "%s\n%s", deststr, STR_DESTINATION_EQUALS_ORIGIN);
            return false;
        }

        // setup buffer
        u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
        if (!buffer) {
            ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
            return false;
        }

        // actual virtual copy operation
        if (force_unmount) DismountDriveType(DriveType(ldest)&(DRV_SYSNAND|DRV_EMUNAND|DRV_IMAGE));
        bool res = PathMoveCopyRec(ldest, lorig, flags, false, buffer, STD_BUFFER_SIZE);
        if (force_unmount) InitExtFS();

        free(buffer);
        return res;
    }
}

bool PathCopy(const char* destdir, const char* orig, u32* flags) {
    // build full destination path (on top of destination directory)
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    if (oname == NULL) return false; // not a proper origin path
    snprintf(dest, 255, "%s/%s", destdir, (++oname));

    // virtual destination special handling
    if (GetVirtualSource(destdir) & ~VRT_BDRI) {
        u64 osize = FileGetSize(orig);
        VirtualFile dvfile;
        if (!osize) return false;
        if (!GetVirtualFile(&dvfile, dest, FA_WRITE)) {
            VirtualDir vdir;
            if (!GetVirtualDir(&vdir, destdir)) return false;
            while (true) { // search by size should be a last resort solution
                if (!ReadVirtualDir(&dvfile, &vdir)) return false;
                if (dvfile.size == osize) break; // file found
            }
            if (!ShowPrompt(true, STR_ENTRY_NOT_FOUND_PATH_INJECT_INTO_PATH_INSTEAD, dest, dvfile.name))
                return false;
            snprintf(dest, 255, "%s/%s", destdir, dvfile.name);
        } else if (osize < dvfile.size) { // if origin is smaller than destination...
            char deststr[UTF_BUFFER_BYTESIZE(36)];
            char origstr[UTF_BUFFER_BYTESIZE(36)];
            char osizestr[32];
            char dsizestr[32];
            TruncateString(deststr, dest, 36, 8);
            TruncateString(origstr, orig, 36, 8);
            FormatBytes(osizestr, osize);
            FormatBytes(dsizestr, dvfile.size);
            if (dvfile.size > osize) {
                if (!ShowPrompt(true, STR_FILE_SMALLER_THAN_SPACE_SIZES_CONTINUE, origstr, osizestr, deststr, dsizestr))
                    return false;
            }
        }
    }

    return PathMoveCopy(dest, orig, flags, false);
}

bool PathMove(const char* destdir, const char* orig, u32* flags) {
    // build full destination path (on top of destination directory)
    char dest[256]; // maximum path name length in FAT
    char* oname = strrchr(orig, '/');
    if (oname == NULL) return false; // not a proper origin path
    snprintf(dest, 255, "%s/%s", destdir, (++oname));

    return PathMoveCopy(dest, orig, flags, true);
}

bool PathDelete(const char* path) {
    if (!CheckDirWritePermissions(path)) return false;
    return (fvx_runlink(path) == FR_OK);
}

bool PathRename(const char* path, const char* newname) {
    char npath[256]; // 256 is the maximum length of a full path
    char* oldname = strrchr(path, '/');

    if (!CheckDirWritePermissions(path)) return false;
    if (!oldname) return false;
    oldname++;
    strncpy(npath, path, oldname - path);
    strncpy(npath + (oldname - path), newname, 255 - (oldname - path));

    if (fvx_rename(path, npath) != FR_OK) return false;
    if ((strncasecmp(path, npath, 256) != 0) &&
        ((fvx_stat(path, NULL) == FR_OK) || (fvx_stat(npath, NULL) != FR_OK)))
        return false; // safety check
    return true;
}

bool PathAttr(const char* path, u8 attr, u8 mask) {
    if (!CheckDirWritePermissions(path)) return false;
    return (f_chmod(path, attr, mask) == FR_OK);
}

bool FileSelectorWorker(char* result, const char* text, const char* path, const char* pattern, u32 flags, void* buffer, bool new_style) {
    DirStruct* contents = (DirStruct*) buffer;
    char path_local[256];
    strncpy(path_local, path, 256);
    path_local[255] = '\0';

    bool no_dirs = flags & NO_DIRS;
    bool no_files = flags & NO_FILES;
    bool hide_ext = flags & HIDE_EXT;
    bool select_dirs = flags & SELECT_DIRS;

    // main loop
    while (true) {
        u32 n_found = 0;
        u32 pos = 0;
        GetDirContents(contents, path_local);

        while (pos < contents->n_entries) {
            char opt_names[_MAX_FS_OPT+1][UTF_BUFFER_BYTESIZE(32)];
            DirEntry* res_entry[MAX_DIR_ENTRIES+1] = { NULL };
            u32 n_opt = 0;
            for (; pos < contents->n_entries; pos++) {
                DirEntry* entry = &(contents->entry[pos]);
                if (((entry->type == T_DIR) && no_dirs) ||
                    ((entry->type == T_FILE) && (no_files || (fvx_match_name(entry->name, pattern) != FR_OK))) ||
                    (entry->type == T_DOTDOT) || (strncmp(entry->name, "._", 2) == 0))
                    continue;
                if (!new_style && n_opt == _MAX_FS_OPT) {
                    snprintf(opt_names[n_opt++], 32, "%s", STR_BRACKET_MORE);
                    break;
                }

                if (!new_style) {
                    char temp_str[256];
                    snprintf(temp_str, 256, "%s", entry->name);
                    if (hide_ext && (entry->type == T_FILE)) {
                        char* dot = strrchr(temp_str, '.');
                        if (dot) *dot = '\0';
                    }
                    TruncateString(opt_names[n_opt], temp_str, 32, 8);
                }
                res_entry[n_opt++] = entry;
                n_found++;
            }
            if ((pos >= contents->n_entries) && (n_opt < n_found) && !new_style)
                snprintf(opt_names[n_opt++], 32, "%s", STR_BRACKET_MORE);
            if (!n_opt) break;

            const char* optionstr[_MAX_FS_OPT+1] = { NULL };
            for (u32 i = 0; i <= _MAX_FS_OPT; i++) optionstr[i] = opt_names[i];
            u32 user_select = new_style ? ShowFileScrollPrompt(n_opt, (const DirEntry**)res_entry, hide_ext, "%s", text)
                                        : ShowSelectPrompt(n_opt, optionstr, "%s", text);
            if (!user_select) return false;
            DirEntry* res_local = res_entry[user_select-1];
            if (res_local && (res_local->type == T_DIR)) { // selected dir
                if (select_dirs) {
                    strncpy(result, res_local->path, 256);
                    return true;
                } else if (FileSelectorWorker(result, text, res_local->path, pattern, flags, buffer, new_style)) {
                    return true;
                }
                break;
            } else if (res_local && (res_local->type == T_FILE)) { // selected file
                strncpy(result, res_local->path, 256);
                return true;
            }
        }
        if (!n_found) { // not a single matching entry found
            char pathstr[UTF_BUFFER_BYTESIZE(32)];
            TruncateString(pathstr, path_local, 32, 8);
            ShowPrompt(false, "%s\n%s", pathstr, STR_NO_USABLE_ENTRIES_FOUND);
            return false;
        }
    }
}

bool FileSelector(char* result, const char* text, const char* path, const char* pattern, u32 flags, bool new_style) {
    void* buffer = (void*) malloc(sizeof(DirStruct));
    if (!buffer) return false;

    // for this to work, result needs to be at least 256 bytes in size
    bool ret = FileSelectorWorker(result, text, path, pattern, flags, buffer, new_style);
    free(buffer);
    return ret;
}
