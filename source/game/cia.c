#include "cia.h"
#include "ff.h"
#include "aes.h"
#include "sha.h"

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

u32 GetTitleKey(u8* titlekey, Ticket* ticket) {
    // From https://github.com/profi200/Project_CTR/blob/master/makerom/pki/prod.h#L19
    static const u8 common_keyy[6][16] = {
        {0xD0, 0x7B, 0x33, 0x7F, 0x9C, 0xA4, 0x38, 0x59, 0x32, 0xA2, 0xE2, 0x57, 0x23, 0x23, 0x2E, 0xB9} , // 0 - eShop Titles
        {0x0C, 0x76, 0x72, 0x30, 0xF0, 0x99, 0x8F, 0x1C, 0x46, 0x82, 0x82, 0x02, 0xFA, 0xAC, 0xBE, 0x4C} , // 1 - System Titles
        {0xC4, 0x75, 0xCB, 0x3A, 0xB8, 0xC7, 0x88, 0xBB, 0x57, 0x5E, 0x12, 0xA1, 0x09, 0x07, 0xB8, 0xA4} , // 2
        {0xE4, 0x86, 0xEE, 0xE3, 0xD0, 0xC0, 0x9C, 0x90, 0x2F, 0x66, 0x86, 0xD4, 0xC0, 0x6F, 0x64, 0x9F} , // 3
        {0xED, 0x31, 0xBA, 0x9C, 0x04, 0xB0, 0x67, 0x50, 0x6C, 0x44, 0x97, 0xA3, 0x5B, 0x78, 0x04, 0xFC} , // 4
        {0x5E, 0x66, 0x99, 0x8A, 0xB4, 0xE8, 0x93, 0x16, 0x06, 0x85, 0x0F, 0xD7, 0xA1, 0x6D, 0xD7, 0x55} , // 5
    };
    // From https://github.com/profi200/Project_CTR/blob/master/makerom/pki/dev.h#L21
    /* static const u8 common_key_devkit[6][16] = { // unused atm!
        {0x55, 0xA3, 0xF8, 0x72, 0xBD, 0xC8, 0x0C, 0x55, 0x5A, 0x65, 0x43, 0x81, 0x13, 0x9E, 0x15, 0x3B} , // 0 - eShop Titles
        {0x44, 0x34, 0xED, 0x14, 0x82, 0x0C, 0xA1, 0xEB, 0xAB, 0x82, 0xC1, 0x6E, 0x7B, 0xEF, 0x0C, 0x25} , // 1 - System Titles
        {0xF6, 0x2E, 0x3F, 0x95, 0x8E, 0x28, 0xA2, 0x1F, 0x28, 0x9E, 0xEC, 0x71, 0xA8, 0x66, 0x29, 0xDC} , // 2
        {0x2B, 0x49, 0xCB, 0x6F, 0x99, 0x98, 0xD9, 0xAD, 0x94, 0xF2, 0xED, 0xE7, 0xB5, 0xDA, 0x3E, 0x27} , // 3
        {0x75, 0x05, 0x52, 0xBF, 0xAA, 0x1C, 0x04, 0x07, 0x55, 0xC8, 0xD5, 0x9A, 0x55, 0xF9, 0xAD, 0x1F} , // 4
        {0xAA, 0xDA, 0x4C, 0xA8, 0xF6, 0xE5, 0xA9, 0x77, 0xE0, 0xA0, 0xF9, 0xE4, 0x76, 0xCF, 0x0D, 0x63} , // 5
    };*/
    
    // setup key 0x3D
    if (ticket->commonkey_idx >= 6) return 1;
    setup_aeskeyY(0x3D, (void*) common_keyy[ticket->commonkey_idx >= 6]);
    use_aeskey(0x3D);
    
    // grab and decrypt the titlekey
    u8 ctr[16] = { 0 };
    memcpy(ctr, ticket->title_id, 8);
    memcpy(titlekey, ticket->titlekey, 16);
    set_ctr(ctr);
    aes_decrypt(titlekey, titlekey, 1, AES_CNT_TITLEKEY_DECRYPT_MODE);
    
    return 0;
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
    const u8 sig_type[4] =  { 0x00, 0x01, 0x00, 0x04 }; // RSA_2048 SHA256
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
    snprintf((char*) ticket->issuer, 0x40, "Root-CA00000003-XS0000000c");
    memset(ticket->ecdsa, 0xFF, 0x3C);
    ticket->version = 0x01;
    memset(ticket->titlekey, 0xFF, 16);
    memcpy(ticket->title_id, title_id, 8);
    ticket->commonkey_idx = 0x01;
    ticket->audit = 0x01; // whatever
    memcpy(ticket->content_index, ticket_cnt_index, sizeof(ticket_cnt_index));
    
    return 0;
}

u32 BuildFakeTmd(TitleMetaData* tmd, u8* title_id, u32 n_contents) {
    const u8 sig_type[4] =  { 0x00, 0x01, 0x00, 0x04 }; // RSA_2048 SHA256
    // safety check: number of contents
    if (n_contents > 64) return 1; // !!!
    // set TMD all zero for a clean start
    memset(tmd, 0x00, sizeof(TitleMetaData) + (n_contents * sizeof(TmdContentChunk)));
    // file TMD values
    memcpy(tmd->sig_type, sig_type, 4);
    memset(tmd->signature, 0xFF, 0x100);
    snprintf((char*) tmd->issuer, 0x40, "Root-CA00000003-CP0000000b");
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

u32 LoadCiaStub(CiaStub* stub, const char* path) {
    FIL file;
    UINT bytes_read;
    CiaInfo info;
    
    if (f_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    // first 0x20 byte of CIA header
    f_lseek(&file, 0);
    if ((f_read(&file, stub, 0x20, &bytes_read) != FR_OK) || (bytes_read != 0x20) ||
        (ValidateCiaHeader(&(stub->header)) != 0)) {
        f_close(&file);
        return 1;
    }
    GetCiaInfo(&info, &(stub->header));
    
    // everything up till content offset
    f_lseek(&file, 0);
    if ((f_read(&file, stub, info.offset_content, &bytes_read) != FR_OK) || (bytes_read != info.offset_content)) {
        f_close(&file);
        return 1;
    }
    
    f_close(&file);
    return 0;
}
