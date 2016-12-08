#include "ncch.h"
#include "keydb.h"
#include "aes.h"
#include "sha.h"
#include "ff.h"

#define SEEDDB_NAME "seeddb.bin"
#define EXEFS_KEYID(name) (((strncmp(name, "banner", 8) == 0) || (strncmp(name, "icon", 8) == 0)) ? 0 : 1)

typedef struct {
    u64 titleId;
    u8 seed[16];
    u8 reserved[8];
} __attribute__((packed)) SeedInfoEntry;

typedef struct {
    u32 n_entries;
    u8 padding[12];
    SeedInfoEntry entries[256]; // this number is only a placeholder
} __attribute__((packed)) SeedInfo;

u32 ValidateNcchHeader(NcchHeader* header) {
    if (memcmp(header->magic, "NCCH", 4) != 0) // check magic number
        return 1;
    
    u32 ncch_units = (NCCH_EXTHDR_OFFSET + header->size_exthdr) / NCCH_MEDIA_UNIT; // exthdr
    if (header->size_plain) { // plain region
        if (header->offset_plain < ncch_units) return 1; // overlapping plain region
        ncch_units = (header->offset_plain + header->size_plain);
    }
    if (header->size_exefs) { // ExeFS
        if (header->offset_exefs < ncch_units) return 1; // overlapping exefs region
        ncch_units = (header->offset_exefs + header->size_exefs);
    }
    if (header->size_romfs) { // RomFS
        if (header->offset_romfs < ncch_units) return 1; // overlapping romfs region
        ncch_units = (header->offset_romfs + header->size_romfs);
    }
    // size check
    if (ncch_units > header->size) return 1; 
     
    return 0;
}

u32 GetNcchCtr(u8* ctr, NcchHeader* ncch, u8 section) {
    memset(ctr, 0x00, 16);
    if (ncch->version == 1) {
        memcpy(ctr, &(ncch->partitionId), 8);
        if (section == 1) { // ExtHeader ctr
            add_ctr(ctr, NCCH_EXTHDR_OFFSET); 
        } else if (section == 2) { // ExeFS ctr
            add_ctr(ctr, ncch->offset_exefs * NCCH_MEDIA_UNIT);
        } else if (section == 3) { // RomFS ctr
            add_ctr(ctr, ncch->offset_romfs * NCCH_MEDIA_UNIT);
        }
    } else {
        for (u32 i = 0; i < 8; i++)
            ctr[i] = ((u8*) &(ncch->partitionId))[7-i];
        ctr[8] = section;
    }
    
    return 0;
}

u32 GetNcchSeed(u8* seed, NcchHeader* ncch) {
    static u8 lseed[16+8] = { 0 }; // seed plus title ID for easy validation
    u64 titleId = ncch->programId;
    u32 hash_seed = ncch->hash_seed;
    
    UINT btr = 0;
    FIL file;
    char path[128];
    u32 sha256sum[8];
    
    memcpy(lseed+16, &(ncch->programId), 8);
    sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
    if (hash_seed == sha256sum[0]) {
        memcpy(seed, lseed, 16);
        return 0;
    }
    
    // try to grab the seed from NAND database
    const char* nand_drv[] = {"1:", "4:"}; // SysNAND and EmuNAND
    for (u32 i = 0; i < (sizeof(nand_drv)/sizeof(char*)); i++) {
        // grab the key Y from movable.sed
        u8 movable_keyy[16];
        snprintf(path, 128, "%s/private/movable.sed", nand_drv[i]);
        if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            continue;
        f_lseek(&file, 0x110);
        f_read(&file, movable_keyy, 0x10, &btr);
        f_close(&file);
        
        // build the seed save path
        sha_quick(sha256sum, movable_keyy, 0x10, SHA256_MODE);
        snprintf(path, 128, "%s/data/%08lX%08lX%08lX%08lX/sysdata/0001000F/00000000",
            nand_drv[i], sha256sum[0], sha256sum[1], sha256sum[2], sha256sum[3]);
            
        // check seedsave for seed
        u8* seedsave = (u8*) TEMP_BUFFER;
        if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            continue;
        f_read(&file, seedsave, 0x200, &btr);
        u32 p_active = (getle32(seedsave + 0x168)) ? 1 : 0;
        static const u32 seed_offset[2] = {0x7000, 0x5C000};
        for (u32 p = 0; p < 2; p++) {
            f_lseek(&file, seed_offset[(p + p_active) % 2]);
            f_read(&file, seedsave, 2000*(8+16), &btr);
            for (u32 s = 0; s < 2000; s++) {
                if (titleId != getle64(seedsave + (s*8)))
                    continue;
                memcpy(lseed, seedsave + (2000*8) + (s*16), 16);
                sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
                if (hash_seed == sha256sum[0]) {
                    memcpy(seed, lseed, 16);
                    f_close(&file);
                    return 0; // found!
                }
            }
        }
        f_close(&file);
    }
    
    // not found -> try seeddb.bin
    const char* base[] = { INPUT_PATHS };
    for (u32 i = 0; i < (sizeof(base)/sizeof(char*)); i++) {
        SeedInfo* seeddb = (SeedInfo*) TEMP_BUFFER;
        snprintf(path, 128, "%s/%s", base[i], SEEDDB_NAME);
        if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
            continue;
        f_read(&file, seeddb, TEMP_BUFFER_SIZE, &btr);
        f_close(&file);
        if (seeddb->n_entries > (btr - 16) / 32)
            continue; // filesize / seeddb size mismatch
        for (u32 s = 0; s < seeddb->n_entries; s++) {
            if (titleId != seeddb->entries[s].titleId)
                continue;
            memcpy(lseed, seeddb->entries[s].seed, 16);
            sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
            if (hash_seed == sha256sum[0]) {
                memcpy(seed, lseed, 16);
                return 0; // found!
            }
        } 
    }
    
    // out of options -> failed!
    return 1;
}

u32 SetNcchKey(NcchHeader* ncch, u32 keyid) {
    u32 keyslot = (!keyid || !ncch->flags[3]) ? 0x2C : // standard / secure3 / secure4 / 7.x crypto
        (ncch->flags[3] == 0x0A) ? 0x18 : (ncch->flags[3] == 0x0B) ? 0x1B : 0x25;
        
    if (!NCCH_ENCRYPTED(ncch))
        return 1;
    
    if (ncch->flags[7] & 0x01) { // fixed key crypto
        // from https://github.com/profi200/Project_CTR/blob/master/makerom/pki/dev.h
        u8 zeroKey[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // zero key
        u8 sysKey[16]  = { 0x52, 0x7C, 0xE6, 0x30, 0xA9, 0xCA, 0x30, 0x5F,
            0x36, 0x96, 0xF3, 0xCD, 0xE9, 0x54, 0x19, 0x4B }; // fixed sys key
        setup_aeskey(0x11, (ncch->programId & ((u64) 0x10 << 32)) ? sysKey : zeroKey);
        use_aeskey(0x11);
        return 0;
    }
    
    // load key X from file if required
    if ((keyslot != 0x2C) && (LoadKeyFromFile(NULL, keyslot, 'X', NULL) != 0))
        return 1;
    
    // key Y for seed and non seed
    if (keyid && (ncch->flags[7] & 0x20)) { // seed crypto
        static u8 seedkeyY[16+16] = { 0 };
        static u8 lsignature[16] = { 0 };
        static u64 ltitleId = 0;
        if ((memcmp(lsignature, ncch->signature, 16) != 0) || (ltitleId != ncch->programId)) {
            u8 keydata[16+16];
            memcpy(keydata, ncch->signature, 16);
            if (GetNcchSeed(keydata + 16, ncch) != 0)
                return 1;
            sha_quick(seedkeyY, keydata, 32, SHA256_MODE);
            memcpy(lsignature, ncch->signature, 16);
            ltitleId = ncch->programId;
        }
        setup_aeskeyY(keyslot, seedkeyY);
    } else { // no seed crypto
        setup_aeskeyY(keyslot, ncch->signature);
    }
    use_aeskey(keyslot);
    
    return 0;
}

u32 CheckNcchCrypto(NcchHeader* ncch) {
    return ((SetNcchKey(ncch, 0) == 0) && (SetNcchKey(ncch, 1) == 0)) ? 0 : 1;
}

u32 DecryptNcchSection(u8* data, u32 offset_data, u32 size_data,
    u32 offset_section, u32 size_section, u32 offset_ctr, NcchHeader* ncch, u32 snum, u32 keyid) {
    const u32 mode = AES_CNT_CTRNAND_MODE;
    
    // check if section in data
    if ((offset_section >= offset_data + size_data) ||
        (offset_data >= offset_section + size_section) ||
        !size_section) {
        return 0; // section not in data
    }
    
    // determine data / offset / size
    u8* data_i = data;
    u32 offset_i = 0;
    u32 size_i = size_section;
    if (offset_section < offset_data)
        offset_i = offset_data - offset_section;
    else data_i = data + (offset_section - offset_data);
    size_i = size_section - offset_i;
    if (size_i > size_data - (data_i - data))
        size_i = size_data - (data_i - data);
    
    // actual decryption stuff
    u8 ctr[16];
    GetNcchCtr(ctr, ncch, snum);
    if (SetNcchKey(ncch, keyid) != 0) return 1;
    ctr_decrypt_byte(data_i, data_i, size_i, offset_i + offset_ctr, mode, ctr);
    
    return 0;
}

// on the fly decryptor for NCCH
u32 DecryptNcch(u8* data, u32 offset, u32 size, NcchHeader* ncch, ExeFsHeader* exefs) {
    const u32 offset_flag3 = 0x188 + 3;
    const u32 offset_flag7 = 0x188 + 7;
    
    // check for encryption
    if (!NCCH_ENCRYPTED(ncch))
        return 0;
    
    // ncch flags handling
    if ((offset <= offset_flag3) && (offset + size > offset_flag3))
        data[offset_flag3 - offset] = 0;
    if ((offset <= offset_flag7) && (offset + size > offset_flag7)) {
        data[offset_flag7 - offset] &= ~(0x01|0x20);
        data[offset_flag7 - offset] |= 0x04;
    }
    
    // exthdr handling
    if (ncch->size_exthdr) {
        if (DecryptNcchSection(data, offset, size,
            NCCH_EXTHDR_OFFSET,
            NCCH_EXTHDR_SIZE,
            0, ncch, 1, 0) != 0) return 1;
    }
    
    // exefs handling
    if (ncch->size_exefs) {
        // exefs header handling
        if (DecryptNcchSection(data, offset, size,
            ncch->offset_exefs * NCCH_MEDIA_UNIT,
            0x200, 0, ncch, 2, 0) != 0) return 1;
        
        // exefs file handling
        if (exefs) for (u32 i = 0; i < 10; i++) {
            ExeFsFileHeader* file = exefs->files + i;
            if (DecryptNcchSection(data, offset, size,
                (ncch->offset_exefs * NCCH_MEDIA_UNIT) + 0x200 + file->offset,
                file->size, 0x200 + file->offset,
                ncch, 2, EXEFS_KEYID(file->name)) != 0) return 1;
        }
    }
    
    // romfs handling
    if (ncch->size_romfs) {
        if (DecryptNcchSection(data, offset, size,
            ncch->offset_romfs * NCCH_MEDIA_UNIT,
            ncch->size_romfs * NCCH_MEDIA_UNIT,
            0, ncch, 3, 1) != 0) return 1;
    }
        
    return 0;
}
