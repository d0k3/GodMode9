#pragma once

#include "common.h"

#define CIA_TICKET_ISSUER   "Root-CA00000003-XS0000000c"
#define CIA_TMD_ISSUER      "Root-CA00000003-CP0000000b"

#define CIA_MAX_CONTENTS    (100+1) // theme CIAs contain maximum 100 themes + 1 index content
#define CIA_HEADER_SIZE     sizeof(CiaHeader)
#define CIA_CERT_SIZE       0xA00
#define CIA_META_SIZE       sizeof(CiaMeta)
#define CIA_TICKET_SIZE     sizeof(Ticket)
#define CIA_TMD_SIZE_MIN    sizeof(TitleMetaData)
#define CIA_TMD_SIZE_MAX    (sizeof(TitleMetaData) + (CIA_MAX_CONTENTS*sizeof(TmdContentChunk)))
#define CIA_TMD_SIZE_N(n)   (sizeof(TitleMetaData) + (n*sizeof(TmdContentChunk)))

// see: https://www.3dbrew.org/wiki/CIA#Meta
typedef struct {
	u8  dependencies[0x180]; // from ExtHeader
    u8  reserved0[0x180];
    u32 core_version; // 2 normally
    u8  reserved1[0xFC];
    u8  smdh[0x36C0]; // from ExeFS
} __attribute__((packed)) CiaMeta;

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

// from: https://github.com/profi200/Project_CTR/blob/02159e17ee225de3f7c46ca195ff0f9ba3b3d3e4/ctrtool/tmd.h#L18-L59;
typedef struct {
	u8 id[4];
	u8 index[2];
    u8 type[2];
    u8 size[8];
    u8 hash[0x20];
} __attribute__((packed)) TmdContentChunk;

typedef struct {
	u8 index[2];
	u8 cmd_count[2];
	u8 hash[0x20];
} __attribute__((packed)) TmdContentInfo;

typedef struct {
    u8 sig_type[4];
    u8 signature[0x100];
	u8 padding[0x3C];
	u8 issuer[0x40];
    u8 version;
    u8 ca_crl_version;
    u8 signer_crl_version;
	u8 reserved0;
	u8 system_version[8];
	u8 title_id[8];
	u8 title_type[4];
	u8 group_id[2];
	u8 save_size[4];
	u8 twl_privsave_size[4];
	u8 reserved1[4];
	u8 twl_flag;
	u8 reserved2[0x31];
	u8 access_rights[4];
	u8 title_version[2];
	u8 content_count[2];
	u8 boot_content[2];
	u8 reserved3[2];
	u8 contentinfo_hash[0x20];
	TmdContentInfo contentinfo[64];
} __attribute__((packed)) TitleMetaData;

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
    u8 ticket_padding[0x40 - (CIA_TICKET_SIZE % 0x40)];
    TitleMetaData tmd;
    TmdContentChunk content_list[CIA_MAX_CONTENTS];
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

typedef struct {
    u64 offset;
    u64 size;
    u32 id;
    u32 index;
    u8  encrypted;
} __attribute__((packed)) CiaContentInfo;

u32 ValidateCiaHeader(CiaHeader* header);
u32 GetCiaInfo(CiaInfo* info, CiaHeader* header);
u32 GetCiaContentInfo(CiaContentInfo* contents, TitleMetaData* tmd);
u32 GetTitleKey(u8* titlekey, Ticket* ticket);
u32 GetTmdCtr(u8* ctr, TmdContentChunk* chunk);
u32 FixTmdHashes(TitleMetaData* tmd);

u32 BuildCiaCert(u8* ciacert);
u32 BuildFakeTicket(Ticket* ticket, u8* title_id);
u32 BuildFakeTmd(TitleMetaData* tmd, u8* title_id, u32 n_contents);

/*u32 BuildCiaMeta(Ncch ncch);
u32 InsertCiaContent(const char* path, const char* content, bool encrypt);*/

u32 DecryptCiaContentSequential(u8* data, u32 size, u8* ctr, const u8* titlekey);
