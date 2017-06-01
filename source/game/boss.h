#pragma once

#include "common.h"

#define BOSS_MAGIC 0x62, 0x6F, 0x73, 0x73, 0x00, 0x01, 0x00, 0x01

#define BOSS_OFFSET_PAYLOAD sizeof(BossHeader)
#define BOSS_SIZE_PAYLOAD_HEADER (0x1C + 2)

// see: http://3dbrew.org/wiki/SpotPass#BOSS_Header
// and: http://3dbrew.org/wiki/SpotPass#Content_Header
// and: http://3dbrew.org/wiki/SpotPass#Payload_Content_Header
// everything is in big endian
typedef struct {
    // actual BOSS header
    u8 magic[8]; // "boss" + 0x00010001, see above
    u8 filesize[4]; // big endian 
    u8 release_date[8];
    u8 unknown0[2]; // always 0x0001
    u8 padding[2];
    u8 cnthdr_hash_type[2]; // always 0x0002
    u8 cnthdr_rsa_size[2]; // always 0x0002
    u8 ctr12[12]; // first 12 byte of ctr
    // content header, encryption starts here (0x28)
    u8 unknown1[0x10]; // usually 0x80 followed by 0x00
    u8 ext_info[2]; // for generating extdata filepath
    u8 hash_header[0x20];
    u8 signature_header[0x100];
    // payload header, first 0x1C byte used for hash (0x15A)
    u8 programId[8]; 
    u8 unknown2[4]; // typically zero
    u8 data_type[4];
    u8 size_payload[4];
    u8 ns_dataId[4];
    u8 unknown3[4];
    u8 hash_payload[0x20];
    u8 signature_payload[0x100];
} __attribute__((packed)) BossHeader;

u32 ValidateBossHeader(BossHeader* header, u32 fsize);
u32 GetBossPayloadHashHeader(u8* header, BossHeader* boss);
u32 CheckBossEncrypted(BossHeader* boss);
u32 CryptBoss(void* data, u32 offset, u32 size, BossHeader* boss);
u32 CryptBossSequential(void* data, u32 offset, u32 size);
