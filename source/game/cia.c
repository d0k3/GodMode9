#include "cia.h"
#include "ncch.h"
#include "exefs.h"
#include "ff.h"
#include "sddata.h"
#include "aes.h"
#include "sha.h"

#define TIKDB_NAME_ENC "encTitleKeys.bin"
#define TIKDB_NAME_DEC "decTitleKeys.bin"

typedef struct {
    u32 commonkey_idx;
    u8  reserved[4];
    u8  title_id[8];
    u8  titlekey[16];
} __attribute__((packed)) TitleKeyEntry;

typedef struct {
    u32 n_entries;
    u8  reserved[12];
    TitleKeyEntry entries[256]; // this number is only a placeholder
} __attribute__((packed)) TitleKeysInfo;

u32 ValidateCiaHeader(CiaHeader* header) {
    if ((header->size_header != CIA_HEADER_SIZE) ||
        (header->size_cert != CIA_CERT_SIZE) ||
        (header->size_ticket != CIA_TICKET_SIZE) ||
        (header->size_tmd < CIA_TMD_SIZE_MIN) ||
        (header->size_tmd > CIA_TMD_SIZE_MAX) ||
        (header->size_content == 0) ||
        ((header->size_meta != 0) && (header->size_meta != CIA_META_SIZE)))
        return 1;
    return 0;
}

u32 GetCiaInfo(CiaInfo* info, CiaHeader* header) {
    memcpy(info, header, 0x20); // take over first 0x20 byte
    
    info->offset_cert = align(header->size_header, 64);
    info->offset_ticket = info->offset_cert + align(header->size_cert, 64);
    info->offset_tmd = info->offset_ticket + align(header->size_ticket, 64);
    info->offset_content = info->offset_tmd + align(header->size_tmd, 64);
    info->offset_meta = (header->size_meta) ? info->offset_content + align(header->size_content, 64) : 0;
    info->offset_content_list = info->offset_tmd + sizeof(TitleMetaData);
    
    info->size_content_list = info->size_tmd - sizeof(TitleMetaData);
    info->size_cia = (header->size_meta) ? info->offset_meta + info->size_meta :
        info->offset_content + info->size_content;
        
    info->max_contents = (info->size_tmd - sizeof(TitleMetaData)) /  sizeof(TmdContentChunk);
    
    return 0;
}

u32 GetCiaContentInfo(CiaContentInfo* contents, TitleMetaData* tmd) {
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    u32 content_count = getbe16(tmd->content_count);
    u64 next_offset = 0;
    for (u32 i = 0; (i < content_count) && (i < CIA_MAX_CONTENTS); i++) {
        contents[i].offset = next_offset;
        contents[i].size = getbe64(content_list[i].size);
        contents[i].id = getbe32(content_list[i].id);
        contents[i].index = getbe16(content_list[i].index);
        contents[i].encrypted = getbe16(content_list[i].type) & 0x1;
        next_offset += contents[i].size;
    }
    
    return 0;
}

u32 CryptTitleKey(TitleKeyEntry* tik, bool encrypt) {
    // From https://github.com/profi200/Project_CTR/blob/master/makerom/pki/prod.h#L19
    static const u8 common_keyy[6][16] __attribute__((aligned(16))) = {
        {0xD0, 0x7B, 0x33, 0x7F, 0x9C, 0xA4, 0x38, 0x59, 0x32, 0xA2, 0xE2, 0x57, 0x23, 0x23, 0x2E, 0xB9} , // 0 - eShop Titles
        {0x0C, 0x76, 0x72, 0x30, 0xF0, 0x99, 0x8F, 0x1C, 0x46, 0x82, 0x82, 0x02, 0xFA, 0xAC, 0xBE, 0x4C} , // 1 - System Titles
        {0xC4, 0x75, 0xCB, 0x3A, 0xB8, 0xC7, 0x88, 0xBB, 0x57, 0x5E, 0x12, 0xA1, 0x09, 0x07, 0xB8, 0xA4} , // 2
        {0xE4, 0x86, 0xEE, 0xE3, 0xD0, 0xC0, 0x9C, 0x90, 0x2F, 0x66, 0x86, 0xD4, 0xC0, 0x6F, 0x64, 0x9F} , // 3
        {0xED, 0x31, 0xBA, 0x9C, 0x04, 0xB0, 0x67, 0x50, 0x6C, 0x44, 0x97, 0xA3, 0x5B, 0x78, 0x04, 0xFC} , // 4
        {0x5E, 0x66, 0x99, 0x8A, 0xB4, 0xE8, 0x93, 0x16, 0x06, 0x85, 0x0F, 0xD7, 0xA1, 0x6D, 0xD7, 0x55} , // 5
    };
    // From https://github.com/profi200/Project_CTR/blob/master/makerom/pki/dev.h#L21
    /* static const u8 common_key_devkit[6][16] __attribute__((aligned(16))) = { // unused atm!
        {0x55, 0xA3, 0xF8, 0x72, 0xBD, 0xC8, 0x0C, 0x55, 0x5A, 0x65, 0x43, 0x81, 0x13, 0x9E, 0x15, 0x3B} , // 0 - eShop Titles
        {0x44, 0x34, 0xED, 0x14, 0x82, 0x0C, 0xA1, 0xEB, 0xAB, 0x82, 0xC1, 0x6E, 0x7B, 0xEF, 0x0C, 0x25} , // 1 - System Titles
        {0xF6, 0x2E, 0x3F, 0x95, 0x8E, 0x28, 0xA2, 0x1F, 0x28, 0x9E, 0xEC, 0x71, 0xA8, 0x66, 0x29, 0xDC} , // 2
        {0x2B, 0x49, 0xCB, 0x6F, 0x99, 0x98, 0xD9, 0xAD, 0x94, 0xF2, 0xED, 0xE7, 0xB5, 0xDA, 0x3E, 0x27} , // 3
        {0x75, 0x05, 0x52, 0xBF, 0xAA, 0x1C, 0x04, 0x07, 0x55, 0xC8, 0xD5, 0x9A, 0x55, 0xF9, 0xAD, 0x1F} , // 4
        {0xAA, 0xDA, 0x4C, 0xA8, 0xF6, 0xE5, 0xA9, 0x77, 0xE0, 0xA0, 0xF9, 0xE4, 0x76, 0xCF, 0x0D, 0x63} , // 5
    };*/
    
    u32 mode = (encrypt) ? AES_CNT_TITLEKEY_ENCRYPT_MODE : AES_CNT_TITLEKEY_DECRYPT_MODE;
    u8 ctr[16] = { 0 };
    
    // setup key 0x3D // ctr
    if (tik->commonkey_idx >= 6) return 1;
    setup_aeskeyY(0x3D, (void*) common_keyy[tik->commonkey_idx]);
    use_aeskey(0x3D);
    memcpy(ctr, tik->title_id, 8);
    set_ctr(ctr);
    
    // decrypt / encrypt the titlekey
    aes_decrypt(tik->titlekey, tik->titlekey, 1, mode);
    return 0;
}

u32 GetTitleKey(u8* titlekey, Ticket* ticket) {
    TitleKeyEntry tik = { 0 };
    memcpy(tik.title_id, ticket->title_id, 8);
    memcpy(tik.titlekey, ticket->titlekey, 16);
    tik.commonkey_idx = ticket->commonkey_idx;
    
    if (CryptTitleKey(&tik, false) != 0) return 0;
    memcpy(titlekey, tik.titlekey, 16);
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

u32 FixCiaHeaderForTmd(CiaHeader* header, TitleMetaData* tmd) {
    TmdContentChunk* content_list = (TmdContentChunk*) (tmd + 1);
    u32 content_count = getbe16(tmd->content_count);
    header->size_content = 0;
    header->size_tmd = CIA_TMD_SIZE_N(content_count);
    memset(header->content_index, 0, sizeof(header->content_index));
    for (u32 i = 0; i < content_count; i++) {
        u16 index = getbe16(content_list[i].index);
        header->size_content += getbe64(content_list[i].size);
        header->content_index[index/8] |= (1 << (7-(index%8)));
    }
    return 0;
}

Ticket* ParseForTicket(u8* data, u32 size, u8* title_id) {
    const u8 magic[] = { CIA_SIG_TYPE };
    for (u32 i = 0; i + sizeof(Ticket) <= size; i++) {
        Ticket* ticket = (Ticket*) (data + i);
        if ((memcmp(ticket->sig_type, magic, sizeof(magic)) != 0) ||
            ((strncmp((char*) ticket->issuer, CIA_TICKET_ISSUER, 0x40) != 0) &&
            (strncmp((char*) ticket->issuer, CIA_TICKET_ISSUER_DEV, 0x40) != 0)))
            continue; // magics not found
        if (title_id && (memcmp(title_id, ticket->title_id, 8) != 0))
            continue; // title id not matching
        return ticket;
    }
    return NULL;
}

u32 BuildCiaCert(u8* ciacert) {
    const u8 cert_hash_expected[0x20] = {
        0xC7, 0x2E, 0x1C, 0xA5, 0x61, 0xDC, 0x9B, 0xC8, 0x05, 0x58, 0x58, 0x9C, 0x63, 0x08, 0x1C, 0x8A,
        0x10, 0x78, 0xDF, 0x42, 0x99, 0x80, 0x3A, 0x68, 0x58, 0xF0, 0x41, 0xF9, 0xCB, 0x10, 0xE6, 0x35
    };
    
    // open certs.db file on SysNAND
    FIL db;
    UINT bytes_read;
    if (f_open(&db, "1:/dbs/certs.db", FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    // grab CIA cert from 4 offsets
    f_lseek(&db, 0x0C10);
    f_read(&db, ciacert + 0x000, 0x1F0, &bytes_read);
    f_lseek(&db, 0x3A00);
    f_read(&db, ciacert + 0x1F0, 0x210, &bytes_read);
    f_lseek(&db, 0x3F10);
    f_read(&db, ciacert + 0x400, 0x300, &bytes_read);
    f_lseek(&db, 0x3C10);
    f_read(&db, ciacert + 0x700, 0x300, &bytes_read);
    f_close(&db);
    
    // check the certificate hash
    u8 cert_hash[0x20];
    sha_quick(cert_hash, ciacert, CIA_CERT_SIZE, SHA256_MODE);
    if (memcmp(cert_hash, cert_hash_expected, 0x20) != 0)
        return 1;
    
    return 0;
}

u32 BuildFakeTicket(Ticket* ticket, u8* title_id) {
    const u8 sig_type[4] =  { CIA_SIG_TYPE }; // RSA_2048 SHA256
    const u8 ticket_cnt_index[] = { // whatever this is
        0x00, 0x01, 0x00, 0x14, 0x00, 0x00, 0x00, 0xAC, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x84,
        0x00, 0x00, 0x00, 0x84, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    // set ticket all zero for a clean start
    memset(ticket, 0x00, sizeof(Ticket));
    // fill ticket values
    memcpy(ticket->sig_type, sig_type, 4);
    memset(ticket->signature, 0xFF, 0x100);
    snprintf((char*) ticket->issuer, 0x40, CIA_TICKET_ISSUER);
    memset(ticket->ecdsa, 0xFF, 0x3C);
    ticket->version = 0x01;
    memset(ticket->titlekey, 0xFF, 16);
    memcpy(ticket->title_id, title_id, 8);
    ticket->commonkey_idx = 0x00; // eshop
    ticket->audit = 0x01; // whatever
    memcpy(ticket->content_index, ticket_cnt_index, sizeof(ticket_cnt_index));
    
    // search for a titlekey inside encTitleKeys.bin / decTitleKeys.bin
    for (u32 enc = 0; enc <= 1; enc++) {
        const char* base[] = { INPUT_PATHS };
        bool found = false;
        for (u32 i = 0; (i < (sizeof(base)/sizeof(char*))) && !found; i++) {
            TitleKeysInfo* tikdb = (TitleKeysInfo*) (TEMP_BUFFER + (TEMP_BUFFER_SIZE/2));
            char path[64];
            FIL file;
            UINT btr;
            
            snprintf(path, 64, "%s/%s", base[i], (enc) ? TIKDB_NAME_ENC : TIKDB_NAME_DEC);
            if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
                continue;
            f_read(&file, tikdb, TEMP_BUFFER_SIZE / 2, &btr);
            f_close(&file);
            if (tikdb->n_entries > (btr - 16) / 32)
                continue; // filesize / titlekey db size mismatch
            for (u32 t = 0; t < tikdb->n_entries; t++) {
                TitleKeyEntry* tik = tikdb->entries + t;
                if (memcmp(title_id, tik->title_id, 8) != 0)
                    continue;
                if (!enc && (CryptTitleKey(tik, true) != 0)) // encrypt the key first
                    continue;
                memcpy(ticket->titlekey, tik->titlekey, 16);
                ticket->commonkey_idx = tik->commonkey_idx;
                found = true; // found, inserted
                break;
            } 
        }
        if (found) break;
    }
    
    return 0;
}

u32 BuildFakeTmd(TitleMetaData* tmd, u8* title_id, u32 n_contents) {
    const u8 sig_type[4] =  { CIA_SIG_TYPE };
    // safety check: number of contents
    if (n_contents > CIA_MAX_CONTENTS) return 1; // !!!
    // set TMD all zero for a clean start
    memset(tmd, 0x00, CIA_TMD_SIZE_N(n_contents));
    // file TMD values
    memcpy(tmd->sig_type, sig_type, 4);
    memset(tmd->signature, 0xFF, 0x100);
    snprintf((char*) tmd->issuer, 0x40, CIA_TMD_ISSUER);
    tmd->version = 0x01;
    memcpy(tmd->title_id, title_id, 8);
    tmd->title_type[3] = 0x40; // whatever
    memset(tmd->save_size, 0x00, 4); // placeholder
    tmd->content_count[1] = (u8) n_contents;
    memset(tmd->contentinfo_hash, 0xFF, 0x20); // placeholder (hash)
    tmd->contentinfo[0].cmd_count[1] = (u8) n_contents;
    memset(tmd->contentinfo[0].hash, 0xFF, 0x20); // placeholder (hash)
    // nothing to do for content list (yet)
    
    return 0;
}

u32 BuildCiaMeta(CiaMeta* meta, u8* exthdr, u8* smdh) {
    // init metadata with all zeroes and core version
    memset(meta, 0x00, sizeof(CiaMeta));
    meta->core_version = 2;
    // copy dependencies from extheader
    if (exthdr) memcpy(meta->dependencies, exthdr + 0x40, sizeof(meta->dependencies));
    // copy smdh (icon file in exefs)
    if (smdh) memcpy(meta->smdh, smdh, sizeof(meta->smdh));
    return 0;
}

u32 BuildCiaHeader(CiaHeader* header) {
    memset(header, 0, sizeof(CiaHeader));
    // sizes in header - fill only known sizes, others zero
    header->size_header = sizeof(CiaHeader);
    header->size_cert = CIA_CERT_SIZE;
    header->size_ticket = sizeof(Ticket);
    header->size_tmd = 0;
    header->size_meta = 0;
    header->size_content = 0;
    return 0;
}

u32 DecryptCiaContentSequential(u8* data, u32 size, u8* ctr, const u8* titlekey) {
    // WARNING: size and offset of data have to be a multiple of 16
    u8 tik[16] __attribute__((aligned(32)));
    u32 mode = AES_CNT_TITLEKEY_DECRYPT_MODE;
    memcpy(tik, titlekey, 16);
    setup_aeskey(0x11, tik);
    use_aeskey(0x11);
    cbc_decrypt(data, data, size / 16, mode, ctr);
    return 0;
}

u32 EncryptCiaContentSequential(u8* data, u32 size, u8* ctr, const u8* titlekey) {
    // WARNING: size and offset of data have to be a multiple of 16
    u8 tik[16] __attribute__((aligned(32)));
    u32 mode = AES_CNT_TITLEKEY_ENCRYPT_MODE;
    memcpy(tik, titlekey, 16);
    setup_aeskey(0x11, tik);
    use_aeskey(0x11);
    cbc_encrypt(data, data, size / 16, mode, ctr);
    return 0;
}
