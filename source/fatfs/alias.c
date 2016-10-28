#include "alias.h"
#include "aes.h"
#include "sha.h"

#define SDCRYPT_BUFFER ((u8*)0x21400000)
#define SDCRYPT_BUFFER_SIZE (0x100000)

#define NUM_ALIAS_DRV 2

char alias_drv[NUM_ALIAS_DRV]; // 1 char ASCII drive number of the alias drive / 0x00 if unused
char alias_path[NUM_ALIAS_DRV][128]; // full path to resolve the alias into

u8 sd_keyy[NUM_ALIAS_DRV][16]; // key Y belonging to alias drive

int alias_num (const TCHAR* path) {
    int num = -1;
    for (u32 i = 0; i < NUM_ALIAS_DRV; i++) {
        if (!alias_drv[i]) continue;
        if ((path[0] == alias_drv[i]) && (path[1] == ':')) {
            num = i;
            break;
        }
    }
    return num;
}

void dealias_path (TCHAR* alias, const TCHAR* path) {
    int num = alias_num(path);
    if (num >= 0) // set alias (alias is assumed to be 256 byte)
        snprintf(alias, 256, "%s%s", alias_path[num], path + 2);
    else strncpy(alias, path, 256);
}

void fx_crypt(XFIL* xfp, void* buff, FSIZE_t off, UINT bt) {
    u32 mode = AES_CNT_CTRNAND_MODE;
    u8 ctr[16] __attribute__((aligned(32)));
    u8 buff16[16];
    u8* buffer = buff;
    
    // copy CTR and increment it
    memcpy(ctr, xfp->iv, 16);
    add_ctr(ctr, off / 16);
    
    // setup the key
    setup_aeskeyY(xfp->keyslot, xfp->keyy);
    use_aeskey(xfp->keyslot);
    
    // handle misaligned offset (at beginning)
    if (off % 16) {
        memcpy(buff16 + (off % 16), buff, 16 - (off % 16));
        ctr_decrypt(buff16, buff16, 1, mode, ctr);
        bt -= 16 - (off % 16);
        buffer += 16 - (off % 16);
    }
    
    // de/encrypt the data
    ctr_decrypt(buff, buff, bt / 16, mode, ctr);
    bt -= 16 * (UINT) (bt / 16);
    buffer += 16 * (UINT) (bt / 16);
    
    // handle misaligned offset (at end)
    if (bt) {
        memcpy(buff16, buff, bt);
        ctr_decrypt(buff16, buff16, 1, mode, ctr);
        buffer += bt;
        bt = 0;
    }
}

FRESULT fx_open (FIL* fp, XFIL* xfp, const TCHAR* path, BYTE mode) {
    int num = alias_num(path);
    xfp->keyslot = 0x40;
    if (num >= 0) {
        // get AES counter, see: http://www.3dbrew.org/wiki/Extdata#Encryption
        // path is the part of the full path after //Nintendo 3DS/<ID0>/<ID1>
        u8 hashstr[256];
        u8 sha256sum[32];
        u32 plen = 0;
        // poor man's UTF-8 -> UTF-16
        for (u32 plen = 0; plen < 128; plen++) {
            hashstr[2*plen] = path[2 + plen];
            hashstr[2*plen+1] = 0;
            if (path[plen] == 0) break;
        }
        sha_quick(sha256sum, hashstr, (plen + 1) * 2, SHA256_MODE);
        for (u32 i = 0; i < 16; i++)
            xfp->iv[i] = sha256sum[i] ^ sha256sum[i+16];
        // copy over key, set keyslot
        memcpy(xfp->keyy, sd_keyy[num], 16);
        xfp->keyslot = 0x34;
    }
    
    return fa_open(fp, path, mode);
}

FRESULT fx_read (FIL* fp, XFIL* xfp, void* buff, UINT btr, UINT* br) {
    FSIZE_t off = f_tell(fp);
    FRESULT res = f_read(fp, buff, btr, br);
    if (xfp && (xfp->keyslot < 0x40))
        fx_crypt(xfp, buff, off, btr);
    return res;
}

FRESULT fx_write (FIL* fp, XFIL* xfp, const void* buff, UINT btw, UINT* bw) {
    FSIZE_t off = f_tell(fp);
    FRESULT res = FR_OK;
    if (xfp && (xfp->keyslot < 0x40)) {
        *bw = 0;
        for (UINT p = 0; (p < btw) && (res == FR_OK); p += SDCRYPT_BUFFER_SIZE) {
            UINT pcount = min(SDCRYPT_BUFFER_SIZE, (btw - p));
            UINT bwl = 0;
            memcpy(SDCRYPT_BUFFER, (u8*) buff + p, pcount);
            fx_crypt(xfp, SDCRYPT_BUFFER, off + p, pcount);
            res = f_write(fp, (const void*) SDCRYPT_BUFFER, pcount, &bwl);
            *bw += bwl;
        }
    } else res = f_write(fp, buff, btw, bw);
    return res;
}

FRESULT fa_open (FIL* fp, const TCHAR* path, BYTE mode) {
    TCHAR alias[256];
    dealias_path(alias, path);
    return f_open(fp, alias, mode);
}

FRESULT fa_opendir (DIR* dp, const TCHAR* path) {
    TCHAR alias[256];
    dealias_path(alias, path);
    return f_opendir(dp, alias);
}

FRESULT fa_stat (const TCHAR* path, FILINFO* fno) {
    TCHAR alias[256];
    dealias_path(alias, path);
    return f_stat(alias, fno);
}

// special functions for access of virtual NAND SD drives
bool SetupNandSdDrive(const char* path, const char* sd_path, const char* movable, int num) {
    char alias[128];
    
    // initial checks
    if ((num >= NUM_ALIAS_DRV) || (num < 0)) return false;
    alias_drv[num] = 0;
    if (!sd_path || !movable || !path) return true;
    
    // grab the key Y from movable.sed
    UINT bytes_read = 0;
    FIL file;
    if (f_open(&file, movable, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return false;
    f_lseek(&file, 0x110);
    if ((f_read(&file, sd_keyy[num], 0x10, &bytes_read) != FR_OK) || (bytes_read != 0x10)) {
        f_close(&file);
        return false;
    }
    f_close(&file);
    
    // build the alias path (id0)
    u32 sha256sum[8];
    sha_quick(sha256sum, sd_keyy[num], 0x10, SHA256_MODE);
    snprintf(alias, 127, "%s/Nintendo 3DS/%08lX%08lX%08lX%08lX",
        sd_path, sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
    
    // find the alias path (id1)
    char* id1 = alias + strnlen(alias, 127);
    DIR pdir;
    FILINFO fno;
    if (f_opendir(&pdir, alias) != FR_OK)
        return false;
    (id1++)[0] = '/';
    *id1 = '\0';
    while (f_readdir(&pdir, &fno) == FR_OK) {
        if (fno.fname[0] == 0)
            break;
        if ((strnlen(fno.fname, 64) != 32) || !(fno.fattrib & AM_DIR))
            continue; // check for id1 directory
        strncpy(id1, fno.fname, 127 - (id1 - alias));
        break;
    }
    f_closedir(&pdir);
    if (!(*id1)) return false;
    
    // create the alias drive
    return SetupAliasDrive(path, alias, num);
}

bool SetupAliasDrive(const char* path, const char* alias, int num) {
    // initial checks
    if ((num >= NUM_ALIAS_DRV) || (num < 0)) return false;
    alias_drv[num] = 0;
    if (!alias || !path) return true;
    
    // take over drive path and alias
    strncpy(alias_path[num], alias, 128);
    if (path[1] != ':') return false;
    alias_drv[num] = path[0];
    
    return true;
}

bool CheckAliasDrive(const char* path) {
    int num = alias_num(path);
    return (num >= 0);
}
