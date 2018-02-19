#include "ticket.h"
#include "unittype.h"
#include "sha.h"
#include "rsa.h"
#include "ff.h"

u32 ValidateTicket(Ticket* ticket) {
    const u8 magic[] = { TICKET_SIG_TYPE };
    if ((memcmp(ticket->sig_type, magic, sizeof(magic)) != 0) ||
        ((strncmp((char*) ticket->issuer, TICKET_ISSUER, 0x40) != 0) &&
        (strncmp((char*) ticket->issuer, TICKET_ISSUER_DEV, 0x40) != 0)) ||
        (ticket->commonkey_idx >= 6))
        return 1;
    return 0;
}

u32 ValidateTicketSignature(Ticket* ticket) {
    static bool got_modexp = false;
    static u8 mod[0x100] = { 0 };
    static u32 exp = 0;
    
    // grab cert from cert.db
    if (!got_modexp) {
        Certificate cert;
        FIL db;
        UINT bytes_read;
        if (f_open(&db, "1:/dbs/certs.db", FA_READ | FA_OPEN_EXISTING) != FR_OK)
            return 1;
        f_lseek(&db, 0x3F10);
        f_read(&db, &cert, CERT_SIZE, &bytes_read);
        f_close(&db);
        memcpy(mod, cert.mod, 0x100);
        exp = getle32(cert.exp);
        got_modexp = true;
    }
    
    if (!RSA_setKey2048(3, mod, exp) ||
        !RSA_verify2048((void*) &(ticket->signature), (void*) &(ticket->issuer), 0x210))
        return 1;
        
    return 0;
}

u32 BuildFakeTicket(Ticket* ticket, u8* title_id) {
    const u8 sig_type[4] =  { TICKET_SIG_TYPE }; // RSA_2048 SHA256
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
    snprintf((char*) ticket->issuer, 0x40, IS_DEVKIT ? TICKET_ISSUER_DEV : TICKET_ISSUER);
    memset(ticket->ecdsa, 0xFF, 0x3C);
    ticket->version = 0x01;
    memset(ticket->titlekey, 0xFF, 16);
    memcpy(ticket->title_id, title_id, 8);
    ticket->commonkey_idx = 0x00; // eshop
    ticket->audit = 0x01; // whatever
    memcpy(ticket->content_index, ticket_cnt_index, sizeof(ticket_cnt_index));
    
    return 0;
}

u32 BuildTicketCert(u8* tickcert) {
    const u8 cert_hash_expected[0x20] = {
        0xDC, 0x15, 0x3C, 0x2B, 0x8A, 0x0A, 0xC8, 0x74, 0xA9, 0xDC, 0x78, 0x61, 0x0E, 0x6A, 0x8F, 0xE3, 
        0xE6, 0xB1, 0x34, 0xD5, 0x52, 0x88, 0x73, 0xC9, 0x61, 0xFB, 0xC7, 0x95, 0xCB, 0x47, 0xE6, 0x97
    };
    const u8 cert_hash_expected_dev[0x20] = {
        0x97, 0x2A, 0x32, 0xFF, 0x9D, 0x4B, 0xAA, 0x2F, 0x1A, 0x24, 0xCF, 0x21, 0x13, 0x87, 0xF5, 0x38,
        0xC6, 0x4B, 0xD4, 0x8F, 0xDF, 0x13, 0x21, 0x3D, 0xFC, 0x72, 0xFC, 0x8D, 0x9F, 0xDD, 0x01, 0x0E
    };
    
    // open certs.db file on SysNAND
    FIL db;
    UINT bytes_read;
    if (f_open(&db, "1:/dbs/certs.db", FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    // grab ticket cert from 3 offsets
    f_lseek(&db, 0x3F10);
    f_read(&db, tickcert + 0x000, 0x300, &bytes_read);
    f_lseek(&db, 0x0C10);
    f_read(&db, tickcert + 0x300, 0x1F0, &bytes_read);
    f_lseek(&db, 0x3A00);
    f_read(&db, tickcert + 0x4F0, 0x210, &bytes_read);
    f_close(&db);
    
    // check the certificate hash
    u8 cert_hash[0x20];
    sha_quick(cert_hash, tickcert, TICKET_CDNCERT_SIZE, SHA256_MODE);
    if (memcmp(cert_hash, IS_DEVKIT ? cert_hash_expected_dev : cert_hash_expected, 0x20) != 0)
        return 1;
    
    return 0;
}
