#pragma once

#include "common.h"
#include "ticket.h"
#include "tmd.h"

#define TADX_HEADER_SIZE    sizeof(TadXHeader)
#define TADX_CERT_SIZE     	0xE80

#define TADX_OFFSET_CERT	align(TADX_HEADER_SIZE, 40)
#define TADX_OFFSET_TICKET	(TADX_OFFSET_CERT   + align(TADX_CERT_SIZE, 40))
#define TADX_OFFSET_TMD		(TADX_OFFSET_TICKET + align(TICKET_TWL_SIZE, 40))
#define TADX_OFFSET_CONTENT	(TADX_OFFSET_TMD    + align(TMD_SIZE_TWL, 40))
#define TADX_OFFSET_CHUNK	(TADX_OFFSET_TMD    + TMD_SIZE_STUB)

// this describes the TWL TAD exchangable format (similar to CIA)
// see: https://wiibrew.org/wiki/WAD_files#WAD_format
typedef struct {
    u8 size_header[4];
    u8 type[2];
    u8 version[2];
    u8 size_cert[4];
    u8 reserved0[4];
    u8 size_ticket[4];
    u8 size_tmd[4];
    u8 size_content[4];
    u8 reserved1[4];
} PACKED_STRUCT TadXHeader;

typedef struct {
    TadXHeader header;
    u8 header_padding[0x40 - (TADX_HEADER_SIZE % 0x40)];
    u8 cert[TADX_CERT_SIZE];
    // cert is aligned and needs no padding
    Ticket ticket;
    u8 ticket_padding[0x40 - (TICKET_TWL_SIZE % 0x40)];
    u8 tmd[TMD_SIZE_TWL];
    u8 tmd_padding[0x40 - (TMD_SIZE_TWL % 40)];
} PACKED_ALIGN(16) TadXStub;
