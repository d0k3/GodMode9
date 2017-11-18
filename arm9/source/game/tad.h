#pragma once

#include "common.h"
#include "nds.h"

#define TAD_NUM_CONTENT     11
#define TAD_HEADER_MAGIC    "3FDT"
#define TAD_BANNER_OFFSET   0
#define TAD_BANNER_LEN      (sizeof(TadBanner) + sizeof(TadBlockMetaData))
#define TAD_HEADER_OFFSET   (TAD_BANNER_OFFSET + TAD_BANNER_LEN)
#define TAD_HEADER_LEN      (sizeof(TadHeader) + sizeof(TadBlockMetaData))


typedef struct {
    u32 banner_end;
    u32 header_end;
    u32 footer_end;
    u32 content_end[TAD_NUM_CONTENT];
} __attribute__((packed)) TadContentTable;

// see: https://www.3dbrew.org/wiki/DSiWare_Exports#Block_Metadata
typedef struct {
    u8 cmac[16];
    u8 iv0[16];
} __attribute__((packed)) TadBlockMetaData;

// see: https://www.3dbrew.org/wiki/DSiWare_Exports#File_Structure_v2
typedef struct {
    TwlIconData icon_data;
    u8 unknown[0x4000 - sizeof(TwlIconData)];
} __attribute__((packed)) TadBanner;

// see: https://www.3dbrew.org/wiki/DSiWare_Exports#Header_2
typedef struct {
    char magic[4]; // "3FDT"
    u16 group_id;
    u16 title_version;
    u8  movable_enc_sha256[0x20];
    u8  cbc_test_block[0x10];
    u64 title_id;
    u64 unknown0;
    u32 content_size[TAD_NUM_CONTENT];
    u8  unknown1[0x30];
    u8  tmd_reserved[0x3E];
    u8  padding[0x0E];
} __attribute__((packed)) TadHeader;

// see: https://www.3dbrew.org/wiki/DSiWare_Exports#Footer
typedef struct {
    u8  banner_sha256[0x20];
    u8  header_sha256[0x20];
    u8  content_sha256[TAD_NUM_CONTENT][0x20];
    u8  ecdsa_signature[0x3C];
    u8  ecdsa_apcert[0x180];
    u8  ecdsa_ctcert[0x180];
    u8  padding[0x4];
} __attribute__((packed)) TadFooter;

u32 BuildTadContentTable(void* table, void* header);
