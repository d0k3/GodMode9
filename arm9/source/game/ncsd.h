#pragma once

#include "common.h"

#define NCSD_MEDIA_UNIT     0x200

#define NCSD_CINFO_OFFSET   0x200
#define NCSD_CINFO_SIZE     0x1000
#define NCSD_DINFO_OFFSET   0x1200
#define NCSD_DINFO_SIZE     0x300
#define NCSD_CNT0_OFFSET    0x4000

// wrapper defines
#define DecryptNcsdSequential(data, offset, size) CryptNcsdSequential(data, offset, size, NCCH_NOCRYPTO)
#define EncryptNcsdSequential(data, offset, size, crypto) CryptNcsdSequential(data, offset, size, crypto)

typedef struct {
    u32 offset;
    u32 size;
} __attribute__((packed)) NcchPartition;

// see: https://www.3dbrew.org/wiki/NCSD#NCSD_header
typedef struct {
    u8  signature[0x100];
    u8  magic[4];
    u32 size;
    u64 mediaId;
    u8  partitions_fs_type[8];
    u8  partitions_crypto_type[8];
    NcchPartition partitions[8];
    u8  hash_exthdr[0x20];
    u8  size_addhdr[0x4];
    u8  sector_zero_offset[0x4];
    u8  partition_flags[8];
    u8  partitionId_table[8][8];
    u8  reserved[0x30];
} __attribute__((packed)) NcsdHeader;

u32 ValidateNcsdHeader(NcsdHeader* header);
u64 GetNcsdTrimmedSize(NcsdHeader* header);
u32 CryptNcsdSequential(void* data, u32 offset_data, u32 size_data, u16 crypto);
