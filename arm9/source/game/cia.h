#pragma once

#include "common.h"
#include "ticket.h"
#include "tmd.h"

#define CIA_HEADER_SIZE     sizeof(CiaHeader)
#define CIA_CERT_SIZE       0xA00
#define CIA_META_SIZE       sizeof(CiaMeta)

// see: https://www.3dbrew.org/wiki/CIA#Meta
typedef struct {
    u8  dependencies[0x180]; // from ExtHeader
    u8  reserved0[0x180];
    u32 core_version; // 2 normally
    u8  reserved1[0xFC];
    u8  smdh[0x36C0]; // from ExeFS
} __attribute__((packed)) CiaMeta;

typedef struct {
    u32 size_header;
    u16 type;
    u16 version;
    u32 size_cert;
    u32 size_ticket;
    u32 size_tmd;
    u32 size_meta;
    u64 size_content;
    u8  content_index[0x2000];
} __attribute__((packed)) CiaHeader;

typedef struct {
    CiaHeader header;
    u8 header_padding[0x40 - (CIA_HEADER_SIZE % 0x40)];
    u8 cert[CIA_CERT_SIZE];
    // cert is aligned and needs no padding
    Ticket ticket;
    u8 ticket_padding[0x40 - (TICKET_SIZE % 0x40)];
    TitleMetaData tmd;
    TmdContentChunk content_list[TMD_MAX_CONTENTS];
} __attribute__((packed)) CiaStub;

typedef struct { // first 0x20 bytes are identical with CIA header
    u32 size_header;
    u16 type;
    u16 version;
    u32 size_cert;
    u32 size_ticket;
    u32 size_tmd;
    u32 size_meta;
    u64 size_content;
    u32 size_content_list;
    u64 size_cia;
    u32 offset_cert;
    u32 offset_ticket;
    u32 offset_tmd;
    u32 offset_content;
    u32 offset_meta;
    u32 offset_content_list;
    u32 max_contents;
} __attribute__((packed)) CiaInfo;

u32 ValidateCiaHeader(CiaHeader* header);
u32 GetCiaInfo(CiaInfo* info, CiaHeader* header);
u32 FixCiaHeaderForTmd(CiaHeader* header, TitleMetaData* tmd);

u32 BuildCiaCert(u8* ciacert);
u32 BuildCiaMeta(CiaMeta* meta, void* exthdr, void* smdh);
u32 BuildCiaHeader(CiaHeader* header);

u32 DecryptCiaContentSequential(void* data, u32 size, u8* ctr, const u8* titlekey);
u32 EncryptCiaContentSequential(void* data, u32 size, u8* ctr, const u8* titlekey);
