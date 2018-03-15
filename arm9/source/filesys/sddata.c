#include "sddata.h"
#include "tad.h"
#include "aes.h"
#include "sha.h"

#define DSIWARE_MAGIC "Nintendo DSiWare" // must be exactly 16 chars
#define NUM_ALIAS_DRV 2
#define NUM_FILCRYPTINFO 16

typedef struct {
    FIL* fptr;
    u8 ctr[16];
    u8 keyy[16];
} __attribute__((packed)) FilCryptInfo;

static FilCryptInfo filcrypt[NUM_FILCRYPTINFO] = { 0 };

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

FilCryptInfo* fx_find_cryptinfo(FIL* fptr) {
    FilCryptInfo* info = NULL;
    
    for (u32 i = 0; i < NUM_FILCRYPTINFO; i++) {
        if (!info && !filcrypt[i].fptr) // use first free
            info = &filcrypt[i];
        if (fptr == filcrypt[i].fptr) {
            info = &filcrypt[i];
            break;
        }
    }
    
    return info;
}

FRESULT fx_decrypt_dsiware (FIL* fp, void* buff, FSIZE_t ofs, UINT len) {
    const u32 mode = AES_CNT_TITLEKEY_DECRYPT_MODE;
    const u32 num_tbl = sizeof(TadContentTable) / sizeof(u32);
    const FSIZE_t ofs0 = f_tell(fp);
    
    u8 __attribute__((aligned(16))) iv[AES_BLOCK_SIZE];
    u32 tbl[num_tbl];
    u8 hdr[TAD_HEADER_LEN];
    
    FRESULT res;
    UINT br;
    
    
    // read and decrypt header
    if ((res = f_lseek(fp, TAD_HEADER_OFFSET)) != FR_OK) return res;
    if ((res = f_read(fp, hdr, TAD_HEADER_LEN, &br)) != FR_OK) return res;
    if (br != TAD_HEADER_LEN) return FR_DENIED;
    memcpy(iv, hdr + TAD_HEADER_LEN - AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    cbc_decrypt(hdr, hdr, sizeof(TadHeader) / AES_BLOCK_SIZE, mode, iv);
    
    // setup the table
    if (BuildTadContentTable(tbl, hdr) != 0) return FR_DENIED;
    if (tbl[num_tbl-1] > f_size(fp)) return FR_DENIED; // obviously missing data
    
    
    // process sections
    u32 sct_start = 0;
    u32 sct_end = 0;
    for (u32 i = 0; i < num_tbl; i++, sct_start = sct_end) {
        sct_end = tbl[i];
        if (sct_start == sct_end) continue; // nothing in section
        if ((ofs + len <= sct_start) || (ofs >= sct_end)) continue; // section not in data
        
        const u32 crypt_end = sct_end - (AES_BLOCK_SIZE * 2);
        const u32 data_end = min(crypt_end, ofs + len);
        u32 data_pos = max(ofs, sct_start);
        if (ofs >= crypt_end) continue; // nothing to do
        
        if ((sct_start < ofs) || (sct_end > ofs + len)) { // incomplete section, ugh
            u8 __attribute__((aligned(16))) block[AES_BLOCK_SIZE];
            
            // load iv0
            FSIZE_t block0_ofs = data_pos - (data_pos % AES_BLOCK_SIZE);
            FSIZE_t iv0_ofs = ((block0_ofs > sct_start) ? block0_ofs : sct_end) - AES_BLOCK_SIZE;
            if ((res = f_lseek(fp, iv0_ofs)) != FR_OK) return res;
            if ((res = f_read(fp, iv, AES_BLOCK_SIZE, &br)) != FR_OK) return res;
            
            // load and decrypt block0 (if misaligned)
            if (data_pos % AES_BLOCK_SIZE) {
                if ((res = f_lseek(fp, block0_ofs)) != FR_OK) return res;
                if ((res = f_read(fp, block, AES_BLOCK_SIZE, &br)) != FR_OK) return res;
                cbc_decrypt(block, block, 1, mode, iv);
                data_pos = min(block0_ofs + AES_BLOCK_SIZE, data_end);
                memcpy(buff, block + (ofs - block0_ofs), data_pos - ofs);
            }
            
            // decrypt blocks in between
            u32 num_blocks = (data_end - data_pos) / AES_BLOCK_SIZE;
            if (num_blocks) {
                u8* blocks = (u8*) buff + (data_pos - ofs);
                cbc_decrypt(blocks, blocks, num_blocks, mode, iv);
                data_pos += num_blocks * AES_BLOCK_SIZE;
            }
            
            // decrypt last block
            if (data_pos < data_end) {
                u8* lbuff = (u8*) buff + (data_pos - ofs);
                // memcpy(block, lbuff, data_end - data_pos); // <--- this should work, but it doesn't
                if ((res = f_lseek(fp, data_pos)) != FR_OK) return res;
                if ((res = f_read(fp, block, AES_BLOCK_SIZE, &br)) != FR_OK) return res;
                cbc_decrypt(block, block, 1, mode, iv);
                memcpy(lbuff, block, data_end - data_pos);
                data_pos = data_end;
            }
        } else { // complete section (thank god for these!)
            u8* blocks = (u8*) buff + (sct_start - ofs);
            u8* iv0 = (u8*) buff + (sct_end - ofs) - AES_BLOCK_SIZE;
            u32 num_blocks = (crypt_end - sct_start) / AES_BLOCK_SIZE;
            memcpy(iv, iv0, AES_BLOCK_SIZE);
            cbc_decrypt(blocks, blocks, num_blocks, mode, iv);
        }
    }
    
    return f_lseek(fp, ofs0);
}

FRESULT fx_open (FIL* fp, const TCHAR* path, BYTE mode) {
    int num = alias_num(path);
    FilCryptInfo* info = fx_find_cryptinfo(fp);
    if (info) info->fptr = NULL;
    
    if (info && (num >= 0)) {
        // DSIWare Export, mark with the magic number
        if (strncmp(path + 2, "/" DSIWARE_MAGIC, 1 + 16) == 0) {
            memcpy(info->ctr, DSIWARE_MAGIC, 16);
        } else {
            // get AES counter, see: http://www.3dbrew.org/wiki/Extdata#Encryption
            // path is the part of the full path after //Nintendo 3DS/<ID0>/<ID1>
            u8 hashstr[256];
            u8 sha256sum[32];
            u32 plen = 0;
            // poor man's ASCII -> UTF-16 / uppercase -> lowercase
            for (plen = 0; plen < 128; plen++) {
                u8 symbol = path[2 + plen];
                if ((symbol >= 'A') && (symbol <= 'Z')) symbol += ('a' - 'A');
                hashstr[2*plen] = symbol;
                hashstr[2*plen+1] = 0;
                if (symbol == 0) break;
            }
            sha_quick(sha256sum, hashstr, (plen + 1) * 2, SHA256_MODE);
            for (u32 i = 0; i < 16; i++)
                info->ctr[i] = sha256sum[i] ^ sha256sum[i+16];
        }
        // copy over key, FIL pointer
        memcpy(info->keyy, sd_keyy[num], 16);
        info->fptr = fp;
    }
    
    return fa_open(fp, path, mode);
}

FRESULT fx_read (FIL* fp, void* buff, UINT btr, UINT* br) {
    FilCryptInfo* info = fx_find_cryptinfo(fp);
    FSIZE_t off = f_tell(fp);
    FRESULT res = f_read(fp, buff, btr, br);
    if (info && info->fptr) {
        setup_aeskeyY(0x34, info->keyy);
        use_aeskey(0x34);
        if (memcmp(info->ctr, DSIWARE_MAGIC, 16) == 0) fx_decrypt_dsiware(fp, buff, off, btr);
        else ctr_decrypt_byte(buff, buff, btr, off, AES_CNT_CTRNAND_MODE, info->ctr);
    }
    return res;
}

FRESULT fx_write (FIL* fp, const void* buff, UINT btw, UINT* bw) {
    FilCryptInfo* info = fx_find_cryptinfo(fp);
    FSIZE_t off = f_tell(fp);
    FRESULT res = FR_OK;
    
    if (info && info->fptr) {
        if (memcmp(info->ctr, DSIWARE_MAGIC, 16) == 0) return FR_DENIED;
        void* crypt_buff = (void*) malloc(min(btw, STD_BUFFER_SIZE));
        if (!crypt_buff) return FR_DENIED;
    
        setup_aeskeyY(0x34, info->keyy);
        use_aeskey(0x34);
        *bw = 0;
        for (UINT p = 0; (p < btw) && (res == FR_OK); p += STD_BUFFER_SIZE) {
            UINT pcount = min(STD_BUFFER_SIZE, (btw - p));
            UINT bwl = 0;
            memcpy(crypt_buff, (u8*) buff + p, pcount);
            ctr_decrypt_byte(crypt_buff, crypt_buff, pcount, off + p, AES_CNT_CTRNAND_MODE, info->ctr);
            res = f_write(fp, (const void*) crypt_buff, pcount, &bwl);
            *bw += bwl;
        }
        
        free(crypt_buff);
    } else res = f_write(fp, buff, btw, bw);
    return res;
}

FRESULT fx_close (FIL* fp) {
    FilCryptInfo* info = fx_find_cryptinfo(fp);
    if (info) memset(info, 0, sizeof(FilCryptInfo));
    return f_close(fp);
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

FRESULT fa_mkdir (const TCHAR* path) {
    TCHAR alias[256];
    dealias_path(alias, path);
    return f_mkdir(alias);
}

FRESULT fa_stat (const TCHAR* path, FILINFO* fno) {
    TCHAR alias[256];
    dealias_path(alias, path);
    return f_stat(alias, fno);
}

FRESULT fa_unlink (const TCHAR* path) {
    TCHAR alias[256];
    dealias_path(alias, path);
    return f_unlink(alias);
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
