#pragma once

#include "common.h"

#define TMD_MAX_CONTENTS    383 // theme CIAs contain maximum 100 themes + 1 index content

#define TMD_SIZE_MIN        sizeof(TitleMetaData)
#define TMD_SIZE_MAX        (sizeof(TitleMetaData) + (TMD_MAX_CONTENTS*sizeof(TmdContentChunk)))
#define TMD_SIZE_N(n)       (sizeof(TitleMetaData) + (n*sizeof(TmdContentChunk)))
#define TMD_CDNCERT_SIZE    0x700

#define TMD_ISSUER          "Root-CA00000003-CP0000000b"
#define TMD_ISSUER_DEV      "Root-CA00000004-CP0000000a"
#define TMD_SIG_TYPE        0x00, 0x01, 0x00, 0x04 // RSA_2048 SHA256

#define DLC_TID_HIGH        0x00, 0x04, 0x00, 0x8C // title id high for DLC

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

u32 ValidateTmd(TitleMetaData* tmd);
u32 GetTmdCtr(u8* ctr, TmdContentChunk* chunk);
u32 FixTmdHashes(TitleMetaData* tmd);
u32 BuildFakeTmd(TitleMetaData* tmd, u8* title_id, u32 n_contents, u32 save_size);
u32 BuildTmdCert(u8* tmdcert);
