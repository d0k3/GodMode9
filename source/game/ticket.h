#pragma once

#include "common.h"

#define TICKET_ISSUER       "Root-CA00000003-XS0000000c"
#define TICKET_ISSUER_DEV   "Root-CA00000004-XS00000009"
#define TICKET_SIG_TYPE     0x00, 0x01, 0x00, 0x04 // RSA_2048 SHA256

#define TIKDB_NAME_ENC      "encTitleKeys.bin"
#define TIKDB_NAME_DEC      "decTitleKeys.bin"

#define TICKDB_AREA_OFFSETS 0x001C0C00, 0x0137F000
#define TICKDB_AREA_SIZE    0x00200000 // the actual area size is around 0x0010C600

// from: https://github.com/profi200/Project_CTR/blob/02159e17ee225de3f7c46ca195ff0f9ba3b3d3e4/ctrtool/tik.h#L15-L39
typedef struct {
    u8 sig_type[4];
	u8 signature[0x100];
	u8 padding1[0x3C];
	u8 issuer[0x40];
	u8 ecdsa[0x3C];
    u8 version;
    u8 ca_crl_version;
    u8 signer_crl_version;
	u8 titlekey[0x10];
	u8 reserved0;
	u8 ticket_id[8];
	u8 console_id[4];
	u8 title_id[8];
	u8 sys_access[2];
	u8 ticket_version[2];
	u8 time_mask[4];
	u8 permit_mask[4];
	u8 title_export;
	u8 commonkey_idx;
    u8 reserved1[0x2A];
    u8 eshop_id[4];
    u8 reserved2;
    u8 audit;
	u8 content_permissions[0x40];
	u8 reserved3[2];
	u8 timelimits[0x40];
    u8 content_index[0xAC];
} __attribute__((packed)) Ticket;

u32 ValidateTicket(Ticket* ticket);
u32 GetTitleKey(u8* titlekey, Ticket* ticket);
u32 GetTicket(Ticket* ticket, u8* title_id, bool force_legit, bool emunand);
u32 BuildFakeTicket(Ticket* ticket, u8* title_id);
