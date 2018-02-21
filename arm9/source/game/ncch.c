#include "ncch.h"
#include "support.h"
#include "disadiff.h"
#include "keydb.h"
#include "aes.h"
#include "sha.h"
#include "ff.h"

#define EXEFS_KEYID(name) (((strncmp(name, "banner", 8) == 0) || (strncmp(name, "icon", 8) == 0)) ? 0 : 1)

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
    u32 sha256sum[8];
    
    memcpy(lseed+16, &(ncch->programId), 8);
    sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
    if (hash_seed == sha256sum[0]) {
        memcpy(seed, lseed, 16);
        return 0;
    }
    
    // setup a large enough buffer
    u8* buffer = (u8*) malloc(max(STD_BUFFER_SIZE, SEEDSAVE_AREA_SIZE));
    if (!buffer) return 1;
    
    // try to grab the seed from NAND database
    const char* nand_drv[] = {"1:", "4:"}; // SysNAND and EmuNAND
    for (u32 i = 0; i < countof(nand_drv); i++) {
        UINT btr = 0;
        FIL file;
        char path[128];
    
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
        u8* seeddb = buffer;
        if (ReadDisaDiffIvfcLvl4(path, NULL, SEEDSAVE_AREA_OFFSET, SEEDSAVE_AREA_SIZE, seeddb) != SEEDSAVE_AREA_SIZE)
            continue;
        
        // search for the seed
        for (u32 s = 0; s < SEEDSAVE_MAX_ENTRIES; s++) {
            if (titleId != getle64(seeddb + (s*8))) continue;
            memcpy(lseed, seeddb + (SEEDSAVE_MAX_ENTRIES*8) + (s*16), 16);
            sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
            if (hash_seed == sha256sum[0]) {
                memcpy(seed, lseed, 16);
                free(buffer);
                return 0; // found!
            }
        }
    }
    
    // not found -> try seeddb.bin
    SeedInfo* seeddb = (SeedInfo*) (void*) buffer;
    size_t len = LoadSupportFile(SEEDDB_NAME, seeddb, STD_BUFFER_SIZE); 
    if (len && (seeddb->n_entries <= (len - 16) / 32)) { // check filesize / seeddb size
        for (u32 s = 0; s < seeddb->n_entries; s++) {
            if (titleId != seeddb->entries[s].titleId)
                continue;
            memcpy(lseed, seeddb->entries[s].seed, 16);
            sha_quick(sha256sum, lseed, 16 + 8, SHA256_MODE);
            if (hash_seed == sha256sum[0]) {
                memcpy(seed, lseed, 16);
                free(buffer);
                return 0; // found!
            }
        }
    }
    
    // out of options -> failed!
    free(buffer);
    return 1;
}

u32 AddSeedToDb(SeedInfo* seed_info, SeedInfoEntry* seed_entry) {
    if (!seed_entry) { // no seed entry -> reset database
        memset(seed_info, 0, 16);
        return 0;
    }
    // check if entry already in DB
    u32 n_entries = seed_info->n_entries;
    SeedInfoEntry* seed = seed_info->entries;
    for (u32 i = 0; i < n_entries; i++, seed++)
        if (seed->titleId == seed_entry->titleId) return 0;
    // actually a new seed entry
    memcpy(seed, seed_entry, sizeof(SeedInfoEntry));
    seed_info->n_entries++;
    return 0;
}

u32 SetNcchKey(NcchHeader* ncch, u16 crypto, u32 keyid) {
    u8 flags3 = (crypto >> 8) & 0xFF;
    u8 flags7 = crypto & 0xFF;
    u32 keyslot = (!keyid || !flags3) ? 0x2C : // standard / secure3 / secure4 / 7.x crypto
        (flags3 == 0x0A) ? 0x18 : (flags3 == 0x0B) ? 0x1B : 0x25;
        
    if (flags7 & 0x04)
        return 1;
    
    if (flags7 & 0x01) { // fixed key crypto
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
    if (keyid && (flags7 & 0x20)) { // seed crypto
        static u8 seedkeyY[16+16] __attribute__((aligned(32))) = { 0 };
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

// this is used to force and check crypto setup
// (also prevents SHA register usage later on) 
u32 SetupNcchCrypto(NcchHeader* ncch, u16 crypt_to) {
    u16 crypt_from = NCCH_GET_CRYPTO(ncch);
    u32 res_from = ((crypt_from & NCCH_NOCRYPTO) ||
        ((SetNcchKey(ncch, crypt_from, 0) == 0) && (SetNcchKey(ncch, crypt_from, 1) == 0))) ? 0 : 1;
    u32 res_to = ((crypt_to & NCCH_NOCRYPTO) ||
        ((SetNcchKey(ncch, crypt_to, 0) == 0) && (SetNcchKey(ncch, crypt_to, 1) == 0))) ? 0 : 1;
    return res_from | res_to;
}

u32 CryptNcchSection(void* data, u32 offset_data, u32 size_data, u32 offset_section, u32 size_section,
    u32 offset_ctr, NcchHeader* ncch, u32 snum, u16 crypt_to, u32 keyid) {
    u16 crypt_from = NCCH_GET_CRYPTO(ncch);
    const u32 mode = AES_CNT_CTRNAND_MODE;
    
    // check if section in data
    if ((offset_section >= offset_data + size_data) ||
        (offset_data >= offset_section + size_section) ||
        !size_section) {
        return 0; // section not in data
    }
    
    // determine data / offset / size
    u8* data8 = (u8*)data;
    u8* data_i = data8;
    u32 offset_i = 0;
    u32 size_i = size_section;
    if (offset_section < offset_data)
        offset_i = offset_data - offset_section;
    else data_i = data8 + (offset_section - offset_data);
    size_i = size_section - offset_i;
    if (size_i > size_data - (data_i - data8))
        size_i = size_data - (data_i - data8);
    
    // actual decryption stuff
    u8 ctr[16];
    GetNcchCtr(ctr, ncch, snum);
    if (!(crypt_from & NCCH_NOCRYPTO)) {
        if (SetNcchKey(ncch, crypt_from, keyid) != 0) return 1;
        ctr_decrypt_byte(data_i, data_i, size_i, offset_i + offset_ctr, mode, ctr);
    }
    if (!(crypt_to & NCCH_NOCRYPTO)) {
        if (SetNcchKey(ncch, crypt_to, keyid) != 0) return 1;
        ctr_decrypt_byte(data_i, data_i, size_i, offset_i + offset_ctr, mode, ctr);
    }
    
    return 0;
}

// on the fly de-/encryptor for NCCH
u32 CryptNcch(void* data, u32 offset, u32 size, NcchHeader* ncch, ExeFsHeader* exefs, u16 crypt_to) {
    const u32 offset_flag3 = 0x188 + 3;
    const u32 offset_flag7 = 0x188 + 7;
    u16 crypt_from = NCCH_GET_CRYPTO(ncch);
    
    // check for encryption
    if ((crypt_to & crypt_from & NCCH_NOCRYPTO) || (crypt_to == crypt_from))
        return 0; // desired end result already met
    
    // ncch flags handling
    if ((offset <= offset_flag3) && (offset + size > offset_flag3))
        ((u8*)data)[offset_flag3 - offset] = (crypt_to >> 8);
    if ((offset <= offset_flag7) && (offset + size > offset_flag7)) {
        ((u8*)data)[offset_flag7 - offset] &= ~(0x01|0x20|0x04);
        ((u8*)data)[offset_flag7 - offset] |= (crypt_to & (0x01|0x20|0x04));
    }
    
    // exthdr handling
    if (ncch->size_exthdr) {
        if (CryptNcchSection(data, offset, size,
            NCCH_EXTHDR_OFFSET,
            NCCH_EXTHDR_SIZE,
            0, ncch, 1, crypt_to, 0) != 0) return 1;
    }
    
    // exefs handling
    if (ncch->size_exefs) {
        // exefs header handling
        if (CryptNcchSection(data, offset, size,
            ncch->offset_exefs * NCCH_MEDIA_UNIT,
            0x200, 0, ncch, 2, crypt_to, 0) != 0) return 1;
            
        // exefs file handling
        if (exefs) for (u32 i = 0; i < 10; i++) {
            ExeFsFileHeader* file = exefs->files + i;
            if (CryptNcchSection(data, offset, size,
                (ncch->offset_exefs * NCCH_MEDIA_UNIT) + 0x200 + file->offset,
                align(file->size, NCCH_MEDIA_UNIT), 0x200 + file->offset,
                ncch, 2, crypt_to, EXEFS_KEYID(file->name)) != 0) return 1;
        }
    }
    
    // romfs handling
    if (ncch->size_romfs) {
        if (CryptNcchSection(data, offset, size,
            ncch->offset_romfs * NCCH_MEDIA_UNIT,
            ncch->size_romfs * NCCH_MEDIA_UNIT,
            0, ncch, 3, crypt_to, 1) != 0) return 1;
    }
    
    return 0;
}

// on the fly de- / encryptor for NCCH - sequential
u32 CryptNcchSequential(void* data, u32 offset, u32 size, u16 crypt_to) {
    // warning: this will only work for sequential processing
    // unexpected results otherwise
    static NcchHeader ncch = { 0 };
    static ExeFsHeader exefs = { 0 };
    static NcchHeader* ncchptr = NULL;
    static ExeFsHeader* exefsptr = NULL;
    
    // fetch ncch header from data
    if ((offset == 0) && (size >= sizeof(NcchHeader))) {
        memcpy(&ncch, data, sizeof(NcchHeader));
        ncchptr = (ValidateNcchHeader(&ncch) == 0) ? &ncch : NULL;
        exefsptr = NULL;
    }
    
    // safety check, ncch pointer
    if (!ncchptr) return 1;
    
    // fetch exefs header from data
    if (ncchptr->offset_exefs && !exefsptr) {
        u32 offset_exefs = ncchptr->offset_exefs * NCCH_MEDIA_UNIT;
        if ((offset <= offset_exefs) &&
            ((offset + size) >= offset_exefs + sizeof(ExeFsHeader))) {
            memcpy(&exefs, (u8*)data + offset_exefs - offset, sizeof(ExeFsHeader));
            if ((NCCH_ENCRYPTED(ncchptr)) && 
                (DecryptNcch((u8*) &exefs, offset_exefs, sizeof(ExeFsHeader), ncchptr, NULL) != 0))
                return 1;
            if (ValidateExeFsHeader(&exefs, 0) != 0) return 1;
            exefsptr = &exefs;
        }
    }
    
    return CryptNcch(data, offset, size, ncchptr, exefsptr, crypt_to);
}

u32 SetNcchSdFlag(void* data) { // data must be at least 0x600 byte and start with NCCH header
    NcchHeader* ncch = (NcchHeader*) data;
    NcchExtHeader* exthdr = (NcchExtHeader*) (void*) ((u8*)data + NCCH_EXTHDR_OFFSET);
    NcchExtHeader exthdr_dec;
    
    if ((ValidateNcchHeader(ncch) != 0) || (!ncch->size_exthdr))
        return 0; // no extheader
    memcpy(&exthdr_dec, exthdr, sizeof(NcchExtHeader));
    if (DecryptNcch((u8*) &exthdr_dec, NCCH_EXTHDR_OFFSET, sizeof(NcchExtHeader), ncch, NULL) != 0)
        return 1;
    if (exthdr_dec.flag & (1<<1)) return 0; // flag already set
    
    exthdr_dec.flag |= (1<<1);
    exthdr->flag ^= (1<<1);
    sha_quick(ncch->hash_exthdr, &exthdr_dec, 0x400, SHA256_MODE);
    
    return 0;
}
