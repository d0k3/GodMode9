#include "ticket.h"
#include "unittype.h"
#include "cert.h"
#include "sha.h"
#include "rsa.h"
#include "ff.h"

u32 ValidateTicket(Ticket* ticket) {
    static const u8 magic[] = { TICKET_SIG_TYPE };
    if ((memcmp(ticket->sig_type, magic, sizeof(magic)) != 0) ||
        ((strncmp((char*) ticket->issuer, TICKET_ISSUER, 0x40) != 0) &&
        (strncmp((char*) ticket->issuer, TICKET_ISSUER_DEV, 0x40) != 0)) ||
        (ticket->commonkey_idx >= 6) ||
        (getbe32(&ticket->content_index[0]) != 0x10014) ||
        (getbe32(&ticket->content_index[4]) < 0x14) ||
        (getbe32(&ticket->content_index[12]) != 0x10014) ||
        (getbe32(&ticket->content_index[16]) != 0))
        return 1;
    return 0;
}

u32 ValidateTwlTicket(Ticket* ticket) {
    static const u8 magic[] = { TICKET_SIG_TYPE_TWL };
    if ((memcmp(ticket->sig_type, magic, sizeof(magic)) != 0) ||
        (strncmp((char*) ticket->issuer, TICKET_ISSUER_TWL, 0x40) != 0))
        return 1;
    return 0;
}

u32 ValidateTicketSignature(Ticket* ticket) {
    Certificate cert;

    // grab cert from certs.db
    if (LoadCertFromCertDb(&cert, (char*)(ticket->issuer)) != 0)
        return 1;

    int ret = Certificate_VerifySignatureBlock(&cert, &(ticket->signature), 0x100, (void*)&(ticket->issuer), GetTicketSize(ticket) - 0x140, true);

    Certificate_Cleanup(&cert);

    return ret;
}

u32 BuildVariableFakeTicket(Ticket** ticket, u32* ticket_size, const u8* title_id, u32 index_max) {
    if (!ticket || !ticket_size)
        return 1;

    static const u8 sig_type[4] =  { TICKET_SIG_TYPE }; // RSA_2048 SHA256

    // calculate sizes and determine pointers to use
    u32 rights_field_count = (min(index_max, 0x10000) + 1023) >> 10; // round up to 1024 and cap at 65536, and div by 1024
    u32 content_index_size = sizeof(TicketContentIndexMainHeader) + sizeof(TicketContentIndexDataHeader) + sizeof(TicketRightsField) * rights_field_count;
    u32 _ticket_size = sizeof(Ticket) + content_index_size;
    Ticket *_ticket;

    if (*ticket) { // if a pointer was pregiven
        if (*ticket_size < _ticket_size) { // then check given boundary size
            *ticket_size = _ticket_size; // if not enough, inform the actual needed size
            return 2; // indicate a size error
        }
        _ticket = *ticket; // get the pointer if we good to go
    } else // if not pregiven, allocate one
        _ticket = (Ticket*)malloc(_ticket_size);

    if (!_ticket)
        return 1;

    // set ticket all zero for a clean start
    memset(_ticket, 0x00, _ticket_size);
    // fill ticket values
    memcpy(_ticket->sig_type, sig_type, 4);
    memset(_ticket->signature, 0xFF, 0x100);
    snprintf((char*) _ticket->issuer, 0x40, IS_DEVKIT ? TICKET_ISSUER_DEV : TICKET_ISSUER);
    memset(_ticket->ecdsa, 0xFF, 0x3C);
    _ticket->version = 0x01;
    memset(_ticket->titlekey, 0xFF, 16);
    if (title_id) memcpy(_ticket->title_id, title_id, 8);
    _ticket->commonkey_idx = 0x00; // eshop
    _ticket->audit = 0x01; // whatever

    // fill in rights
    TicketContentIndexMainHeader* mheader = (TicketContentIndexMainHeader*)&_ticket->content_index[0];
    TicketContentIndexDataHeader* dheader = (TicketContentIndexDataHeader*)&_ticket->content_index[0x14];
    TicketRightsField* rights = (TicketRightsField*)&_ticket->content_index[0x28];

    // first main data header
    mheader->unk1[1] = 0x1; mheader->unk2[1] = 0x14;
    mheader->content_index_size[3] = (u8)(content_index_size >>  0);
    mheader->content_index_size[2] = (u8)(content_index_size >>  8);
    mheader->content_index_size[1] = (u8)(content_index_size >> 16);
    mheader->content_index_size[0] = (u8)(content_index_size >> 24);
    mheader->data_header_relative_offset[3] = 0x14; // relative offset for TicketContentIndexDataHeader
    mheader->unk3[1] = 0x1; mheader->unk4[1] = 0x14;

    // then the data header
    dheader->data_relative_offset[3] = 0x28; // relative offset for TicketRightsField
    dheader->max_entry_count[3] = (u8)(rights_field_count >>  0);
    dheader->max_entry_count[2] = (u8)(rights_field_count >>  8);
    dheader->max_entry_count[1] = (u8)(rights_field_count >> 16);
    dheader->max_entry_count[0] = (u8)(rights_field_count >> 24);
    dheader->size_per_entry[3] = (u8)sizeof(TicketRightsField); // sizeof should be 0x84
    dheader->total_size_used[3] = (u8)((sizeof(TicketRightsField) * rights_field_count) >>  0);
    dheader->total_size_used[2] = (u8)((sizeof(TicketRightsField) * rights_field_count) >>  8);
    dheader->total_size_used[1] = (u8)((sizeof(TicketRightsField) * rights_field_count) >> 16);
    dheader->total_size_used[0] = (u8)((sizeof(TicketRightsField) * rights_field_count) >> 24);
    dheader->data_type[1] = 3; // right fields

    // now the right fields
    // indexoffets must be in accending order to have the desired effect
    for (u32 i = 0; i < rights_field_count; ++i) {
        rights[i].indexoffset[1] = (u8)((1024 * i) >> 0);
        rights[i].indexoffset[0] = (u8)((1024 * i) >> 8);
        memset(&rights[i].rightsbitfield[0], 0xFF, sizeof(rights[0].rightsbitfield));
    }

    *ticket = _ticket;
    *ticket_size = _ticket_size;

    return 0;
}

u32 BuildFakeTicket(Ticket* ticket, const u8* title_id) {
    Ticket* tik;
    u32 ticket_size = sizeof(TicketCommon);
    u32 res = BuildVariableFakeTicket(&tik, &ticket_size, title_id, TICKET_MAX_CONTENTS);
    if (res != 0) return res;
    memcpy(ticket, tik, ticket_size);
    free(tik);
    return 0;
}

u32 GetTicketContentIndexSize(const Ticket* ticket) {
    return getbe32(&ticket->content_index[4]);
}

u32 GetTicketSize(const Ticket* ticket) {
    return sizeof(Ticket) + GetTicketContentIndexSize(ticket);
}

u32 BuildTicketCert(u8* tickcert) {
    static const u8 cert_hash_expected[0x20] = {
        0xDC, 0x15, 0x3C, 0x2B, 0x8A, 0x0A, 0xC8, 0x74, 0xA9, 0xDC, 0x78, 0x61, 0x0E, 0x6A, 0x8F, 0xE3,
        0xE6, 0xB1, 0x34, 0xD5, 0x52, 0x88, 0x73, 0xC9, 0x61, 0xFB, 0xC7, 0x95, 0xCB, 0x47, 0xE6, 0x97
    };
    static const u8 cert_hash_expected_dev[0x20] = {
        0x97, 0x2A, 0x32, 0xFF, 0x9D, 0x4B, 0xAA, 0x2F, 0x1A, 0x24, 0xCF, 0x21, 0x13, 0x87, 0xF5, 0x38,
        0xC6, 0x4B, 0xD4, 0x8F, 0xDF, 0x13, 0x21, 0x3D, 0xFC, 0x72, 0xFC, 0x8D, 0x9F, 0xDD, 0x01, 0x0E
    };

    static const char* const retail_issuers[] = {"Root-CA00000003-XS0000000c", "Root-CA00000003"};
    static const char* const dev_issuers[] = {"Root-CA00000004-XS00000009", "Root-CA00000004"};

    size_t size = TICKET_CDNCERT_SIZE;
    if (BuildRawCertBundleFromCertDb(tickcert, &size, !IS_DEVKIT ? retail_issuers : dev_issuers, 2) ||
        size != TICKET_CDNCERT_SIZE) {
        return 1;
    }

    // check the certificate hash
    u8 cert_hash[0x20];
    sha_quick(cert_hash, tickcert, TICKET_CDNCERT_SIZE, SHA256_MODE);
    if (memcmp(cert_hash, IS_DEVKIT ? cert_hash_expected_dev : cert_hash_expected, 0x20) != 0)
        return 1;

    return 0;
}

u32 TicketRightsCheck_InitContext(TicketRightsCheck* ctx, Ticket* ticket) {
    if (!ticket || ValidateTicket(ticket)) return 1;

    const TicketContentIndexMainHeader* mheader = (const TicketContentIndexMainHeader*)&ticket->content_index[0];
    u32 dheader_pos = getbe32(&mheader->data_header_relative_offset[0]);
    u32 cindex_size = getbe32(&mheader->content_index_size[0]);

    // data header is not inbounds, so it's not valid for use
    if (cindex_size < dheader_pos || dheader_pos + sizeof(TicketContentIndexDataHeader) > cindex_size) return 1;

    const TicketContentIndexDataHeader* dheader = (const TicketContentIndexDataHeader*)&ticket->content_index[dheader_pos];
    u32 data_pos = getbe32(&dheader->data_relative_offset[0]);
    u32 count = getbe32(&dheader->max_entry_count[0]);
    u32 data_max_size = cindex_size - data_pos;

    count = min(data_max_size / sizeof(TicketRightsField), count);

    // if no entries or data type isn't what we want or not enough space for at least one entry,
    // it still is valid, but it will just follow other rules
    if (count == 0 || getbe16(&dheader->data_type[0]) != 3) {
        ctx->count = 0;
        ctx->rights = NULL;
    } else {
        ctx->count = count;
        ctx->rights = (const TicketRightsField*)&ticket->content_index[data_pos];
    }

    return 0;
}

bool TicketRightsCheck_CheckIndex(TicketRightsCheck* ctx, u16 index) {
    if (ctx->count == 0) return index < 256; // when no fields, true if below 256

    bool hasright = false;

    // it loops until one of these happens:
    // - we run out of bit fields
    // - at the first encounter of an index offset field that's bigger than index
    // - at the first encounter of a positive indicator of content rights
    for (u32 i = 0; i < ctx->count; i++) {
        u16 indexoffset = getbe16(&ctx->rights[i].indexoffset[0]);
        if (index < indexoffset) break;
        u16 bitpos = index - indexoffset;
        if (bitpos >= 1024) continue; // not in this field
        if (ctx->rights[i].rightsbitfield[bitpos / 8] & (1 << (bitpos % 8))) {
            hasright = true;
            break;
        }
    }

    return hasright;
}
