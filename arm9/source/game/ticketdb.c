#include "ticketdb.h"
#include "disadiff.h"
#include "support.h"
#include "aes.h"
#include "ff.h"


u32 CryptTitleKey(TitleKeyEntry* tik, bool encrypt, bool devkit) {
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
    static const u8 common_key_dev[6][16] __attribute__((aligned(16))) = {
        {0x55, 0xA3, 0xF8, 0x72, 0xBD, 0xC8, 0x0C, 0x55, 0x5A, 0x65, 0x43, 0x81, 0x13, 0x9E, 0x15, 0x3B} , // 0 - eShop Titles
        {0x44, 0x34, 0xED, 0x14, 0x82, 0x0C, 0xA1, 0xEB, 0xAB, 0x82, 0xC1, 0x6E, 0x7B, 0xEF, 0x0C, 0x25} , // 1 - System Titles
        {0xF6, 0x2E, 0x3F, 0x95, 0x8E, 0x28, 0xA2, 0x1F, 0x28, 0x9E, 0xEC, 0x71, 0xA8, 0x66, 0x29, 0xDC} , // 2
        {0x2B, 0x49, 0xCB, 0x6F, 0x99, 0x98, 0xD9, 0xAD, 0x94, 0xF2, 0xED, 0xE7, 0xB5, 0xDA, 0x3E, 0x27} , // 3
        {0x75, 0x05, 0x52, 0xBF, 0xAA, 0x1C, 0x04, 0x07, 0x55, 0xC8, 0xD5, 0x9A, 0x55, 0xF9, 0xAD, 0x1F} , // 4
        {0xAA, 0xDA, 0x4C, 0xA8, 0xF6, 0xE5, 0xA9, 0x77, 0xE0, 0xA0, 0xF9, 0xE4, 0x76, 0xCF, 0x0D, 0x63} , // 5
    };
    
    u32 mode = (encrypt) ? AES_CNT_TITLEKEY_ENCRYPT_MODE : AES_CNT_TITLEKEY_DECRYPT_MODE;
    u8 ctr[16] = { 0 };
    
    // setup key 0x3D // ctr
    if (tik->commonkey_idx >= 6) return 1;
    if (!devkit) setup_aeskeyY(0x3D, (void*) common_keyy[tik->commonkey_idx]);
    else setup_aeskey(0x3D, (void*) common_key_dev[tik->commonkey_idx]);
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
    
    if (CryptTitleKey(&tik, false, TICKET_DEVKIT(ticket)) != 0) return 1;
    memcpy(titlekey, tik.titlekey, 16);
    return 0;
}

Ticket* TicketFromTickDbChunk(u8* chunk, u8* title_id, bool legit_pls) {
    // chunk must be aligned to 0x200 byte in file and at least 0x400 byte big
    Ticket* tick = (Ticket*) (chunk + 0x18);
    if ((getle32(chunk + 0x10) == 0) || (getle32(chunk + 0x14) != sizeof(Ticket))) return NULL;
    if (ValidateTicket(tick) != 0) return NULL; // ticket not validated
    if (title_id && (memcmp(title_id, tick->title_id, 8) != 0)) return NULL; // title id not matching
    if (legit_pls && (ValidateTicketSignature(tick) != 0)) return NULL; // legit check using RSA sig
    
    return tick;
}

u32 FindTicket(Ticket* ticket, u8* title_id, bool force_legit, bool emunand) {
    const char* path_db = TICKDB_PATH(emunand); // EmuNAND / SysNAND
    u8* data = (u8*) malloc(TICKDB_AREA_SIZE);
    if (!data) return 1;
    
    // read and decode ticket.db DIFF partition
    if (ReadDisaDiffIvfcLvl4(path_db, NULL, TICKDB_AREA_OFFSET, TICKDB_AREA_SIZE, data) != TICKDB_AREA_SIZE) {
        free(data);
        return 1;
    }
    
    // parse the decoded data for a ticket
    bool found = false;
    for (u32 i = 0; !found && (i < TICKDB_AREA_SIZE + 0x400); i += 0x200) {
        Ticket* tick = TicketFromTickDbChunk(data + i, title_id, force_legit);
        if (!tick) continue;
        memcpy(ticket, tick, sizeof(Ticket));
        found = true;
    }
    
    free(data);
    return (found) ? 0 : 1;
}

u32 FindTitleKey(Ticket* ticket, u8* title_id) {
    bool found = false;
    TitleKeysInfo* tikdb = (TitleKeysInfo*) malloc(STD_BUFFER_SIZE); // more than enough
    if (!tikdb) return 1;
    
    // search for a titlekey inside encTitleKeys.bin / decTitleKeys.bin
    // when found, add it to the ticket
    for (u32 enc = 0; (enc <= 1) && !found; enc++) {
        u32 len = LoadSupportFile((enc) ? TIKDB_NAME_ENC : TIKDB_NAME_DEC, tikdb, STD_BUFFER_SIZE);
        
        if (len == 0) continue; // file not found
        if (tikdb->n_entries > (len - 16) / 32)
            continue; // filesize / titlekey db size mismatch
        for (u32 t = 0; t < tikdb->n_entries; t++) {
            TitleKeyEntry* tik = tikdb->entries + t;
            if (memcmp(title_id, tik->title_id, 8) != 0)
                continue;
            if (!enc && (CryptTitleKey(tik, true, TICKET_DEVKIT(ticket)) != 0)) // encrypt the key first
                continue;
            memcpy(ticket->titlekey, tik->titlekey, 16);
            ticket->commonkey_idx = tik->commonkey_idx;
            found = true; // found, inserted
            break;
        }
    }
    
    free(tikdb);
    return (found) ? 0 : 1;
}

u32 AddTitleKeyToInfo(TitleKeysInfo* tik_info, TitleKeyEntry* tik_entry, bool decrypted_in, bool decrypted_out, bool devkit) {
    if (!tik_entry) { // no titlekey entry -> reset database
        memset(tik_info, 0, 16);
        return 0;
    }
    // check if entry already in DB
    u32 n_entries = tik_info->n_entries;
    TitleKeyEntry* tik = tik_info->entries;
    for (u32 i = 0; i < n_entries; i++, tik++)
        if (memcmp(tik->title_id, tik_entry->title_id, 8) == 0) return 0;
    // actually a new titlekey
    memcpy(tik, tik_entry, sizeof(TitleKeyEntry));
    if ((decrypted_in != decrypted_out) && (CryptTitleKey(tik, !decrypted_out, devkit) != 0)) return 1;
    tik_info->n_entries++;
    return 0;
}

u32 AddTicketToInfo(TitleKeysInfo* tik_info, Ticket* ticket, bool decrypt) { // TODO: check for legit tickets?
    if (!ticket) return AddTitleKeyToInfo(tik_info, NULL, false, false, false);
    TitleKeyEntry tik = { 0 };
    memcpy(tik.title_id, ticket->title_id, 8);
    memcpy(tik.titlekey, ticket->titlekey, 16);
    tik.commonkey_idx = ticket->commonkey_idx;
    return AddTitleKeyToInfo(tik_info, &tik, false, decrypt, TICKET_DEVKIT(ticket));
}
