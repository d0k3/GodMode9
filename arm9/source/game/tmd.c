#include "tmd.h"
#include "unittype.h"
#include "sha.h"
#include "ff.h"

u32 ValidateTmd(TitleMetaData* tmd) {
    const u8 magic[] = { TMD_SIG_TYPE };
    if ((memcmp(tmd->sig_type, magic, sizeof(magic)) != 0) ||
        ((strncmp((char*) tmd->issuer, TMD_ISSUER, 0x40) != 0) &&
        (strncmp((char*) tmd->issuer, TMD_ISSUER_DEV, 0x40) != 0)))
        return 1;
    return 0;
}

u32 GetTmdCtr(u8* ctr, TmdContentChunk* chunk) {
    memset(ctr, 0, 16);
    memcpy(ctr, chunk->index, 2);
    return 0;
}

u32 FixTmdHashes(TitleMetaData* tmd) {
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    u32 content_count = getbe16(tmd->content_count);
    // recalculate content info hashes
    for (u32 i = 0, kc = 0; i < 64 && kc < content_count; i++) {
        TmdContentInfo* info = tmd->contentinfo + i;
        u32 k = getbe16(info->cmd_count);
        sha_quick(info->hash, content_list + kc, k * sizeof(TmdContentChunk), SHA256_MODE);
        kc += k;
    }
    sha_quick(tmd->contentinfo_hash, (u8*)tmd->contentinfo, 64 * sizeof(TmdContentInfo), SHA256_MODE);
    return 0;
}

u32 BuildFakeTmd(TitleMetaData* tmd, u8* title_id, u32 n_contents, u32 save_size) {
    const u8 sig_type[4] =  { TMD_SIG_TYPE };
    // safety check: number of contents
    if (n_contents > TMD_MAX_CONTENTS) return 1; // potential incompatibility here (!)
    // set TMD all zero for a clean start
    memset(tmd, 0x00, TMD_SIZE_N(n_contents));
    // file TMD values
    memcpy(tmd->sig_type, sig_type, 4);
    memset(tmd->signature, 0xFF, 0x100);
    snprintf((char*) tmd->issuer, 0x40, IS_DEVKIT ? TMD_ISSUER_DEV : TMD_ISSUER);
    tmd->version = 0x01;
    memcpy(tmd->title_id, title_id, 8);
    tmd->title_type[3] = 0x40; // whatever
    for (u32 i = 0; i < 4; i++) tmd->save_size[i] = (save_size >> (i*8)) & 0xFF; // little endian?
    tmd->content_count[0] = (u8) ((n_contents >> 8) & 0xFF);
    tmd->content_count[1] = (u8) (n_contents & 0xFF);
    memset(tmd->contentinfo_hash, 0xFF, 0x20); // placeholder (hash)
    memcpy(tmd->contentinfo[0].cmd_count, tmd->content_count, 2);
    memset(tmd->contentinfo[0].hash, 0xFF, 0x20); // placeholder (hash)
    // nothing to do for content list (yet)
    
    return 0;
}

u32 BuildTmdCert(u8* tmdcert) {
    const u8 cert_hash_expected[0x20] = {
        0x91, 0x5F, 0x77, 0x3A, 0x07, 0x82, 0xD4, 0x27, 0xC4, 0xCE, 0xF5, 0x49, 0x25, 0x33, 0xE8, 0xEC, 
        0xF6, 0xFE, 0xA1, 0xEB, 0x8C, 0xCF, 0x59, 0x6E, 0x69, 0xBA, 0x2A, 0x38, 0x8D, 0x73, 0x8A, 0xE1
    };
    const u8 cert_hash_expected_dev[0x20] = {
        0x49, 0xC9, 0x41, 0x56, 0xCA, 0x86, 0xBD, 0x1F, 0x36, 0x51, 0x51, 0x6A, 0x4A, 0x9F, 0x54, 0xA1,
        0xC2, 0xE9, 0xCA, 0x93, 0x94, 0xF4, 0x29, 0xA0, 0x38, 0x54, 0x75, 0xFF, 0xAB, 0x6E, 0x8E, 0x71
    };
    
    // open certs.db file on SysNAND
    FIL db;
    UINT bytes_read;
    if (f_open(&db, "1:/dbs/certs.db", FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    // grab TMD cert from 3 offsets
    f_lseek(&db, 0x3C10);
    f_read(&db, tmdcert + 0x000, 0x300, &bytes_read);
    f_lseek(&db, 0x0C10);
    f_read(&db, tmdcert + 0x300, 0x1F0, &bytes_read);
    f_lseek(&db, 0x3A00);
    f_read(&db, tmdcert + 0x4F0, 0x210, &bytes_read);
    f_close(&db);
    
    // check the certificate hash
    u8 cert_hash[0x20];
    sha_quick(cert_hash, tmdcert, TMD_CDNCERT_SIZE, SHA256_MODE);
    if (memcmp(cert_hash, IS_DEVKIT ? cert_hash_expected_dev : cert_hash_expected, 0x20) != 0)
        return 1;
    
    return 0;
}
