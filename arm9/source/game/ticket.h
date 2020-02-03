#pragma once

#include "common.h"

#define TICKET_COMMON_SIZE  sizeof(TicketCommon)
#define TICKET_MINIMUM_SIZE sizeof(TicketMinimum)
#define TICKET_CDNCERT_SIZE 0x700

#define TICKET_ISSUER       "Root-CA00000003-XS0000000c"
#define TICKET_ISSUER_DEV   "Root-CA00000004-XS00000009"
#define TICKET_SIG_TYPE     0x00, 0x01, 0x00, 0x04 // RSA_2048 SHA256

#define TICKET_DEVKIT(tick) (strncmp((char*)tick->issuer, TICKET_ISSUER_DEV, 0x40) == 0)

// from: https://github.com/profi200/Project_CTR/blob/02159e17ee225de3f7c46ca195ff0f9ba3b3d3e4/ctrtool/tik.h#L15-L39
// all numbers in big endian

#define TICKETBASE \
    u8 sig_type[4]; \
    u8 signature[0x100]; \
    u8 padding1[0x3C]; \
    u8 issuer[0x40]; \
    u8 ecdsa[0x3C]; \
    u8 version; \
    u8 ca_crl_version; \
    u8 signer_crl_version; \
    u8 titlekey[0x10]; \
    u8 reserved0; \
    u8 ticket_id[8]; \
    u8 console_id[4]; \
    u8 title_id[8]; \
    u8 sys_access[2]; \
    u8 ticket_version[2]; \
    u8 time_mask[4]; \
    u8 permit_mask[4]; \
    u8 title_export; \
    u8 commonkey_idx; \
    u8 reserved1[0x2A]; \
    u8 eshop_id[4]; \
    u8 reserved2; \
    u8 audit; \
    u8 content_permissions[0x40]; \
    u8 reserved3[2]; \
    u8 timelimits[0x40]

typedef struct {
    TICKETBASE;
    u8 content_index[];
} __attribute__((packed, aligned(4))) Ticket;

typedef struct {
    TICKETBASE;
    u8 content_index[0xAC];
} __attribute__((packed, aligned(4))) TicketCommon;

// minimum allowed content_index is 0x14
typedef struct {
    TICKETBASE;
    u8 content_index[0x14];
} __attribute__((packed, aligned(4))) TicketMinimum;

typedef struct {
    u8 unk1[2];
    u8 unk2[2];
    u8 content_index_size[4];
    u8 data_header_relative_offset[4]; // relative to content index start
    u8 unk3[2];
    u8 unk4[2];
    u8 unk5[4];
} __attribute__((packed)) TicketContentIndexMainHeader;

typedef struct {
    u8 data_relative_offset[4]; // relative to content index start
    u8 max_entry_count[4];
    u8 size_per_entry[4]; // but has no effect
    u8 total_size_used[4]; // also no effect
    u8 data_type[2]; // perhaps, does have effect and change with different data like on 0004000D tickets
    u8 unknown[2]; // or padding
} __attribute__((packed)) TicketContentIndexDataHeader;

// data type == 3
typedef struct {
    u8 unk[2]; // seemly has no meaning
    u8 indexoffset[2];
    u8 rightsbitfield[0x80];
} __attribute__((packed)) TicketRightsField;

typedef struct {
    size_t count;
    const TicketRightsField* rights; // points within ticket pointer
} TicketRightsCheck;

u32 ValidateTicket(Ticket* ticket);
u32 ValidateTicketSignature(Ticket* ticket);
u32 BuildFakeTicket(Ticket* ticket, u8* title_id);
u32 GetTicketContentIndexSize(const Ticket* ticket);
u32 GetTicketSize(const Ticket* ticket);
u32 BuildTicketCert(u8* tickcert);
u32 TicketRightsCheck_InitContext(TicketRightsCheck* ctx, Ticket* ticket);
bool TicketRightsCheck_CheckIndex(TicketRightsCheck* ctx, u16 index);
