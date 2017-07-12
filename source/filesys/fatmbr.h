#pragma once

#include "common.h"

#define FATMBR_MAGIC    0xAA55 // little endian!

typedef struct {
    u8  status;         // 0x80
    u8  chs_start[3];   // 0x01 0x01 0x00
    u8  type;           // 0x0C
    u8  chs_end[3];     // 0xFE 0xFF 0xFF
    u32 sector;         // 0x2000 (4MB offset, 512 byte sectors)
    u32 count;
} __attribute__((packed)) MbrPartitionInfo;

typedef struct {
    char text[446];
    MbrPartitionInfo partitions[4]; 
    u16  magic;         // 0xAA55
} __attribute__((packed)) MbrHeader;

typedef struct { // unused
    u32 signature0;     // 0x41615252
    u8  reserved0[480];
    u32 signature1;     // 0x61417272
    u32 clr_free;       // 0xFFFFFFFF
    u32 clr_next;       // 0xFFFFFFFF
    u8  reserved1[14];
    u16 magic;          // 0xAA55
} __attribute__((packed)) FileSystemInfo;

typedef struct {
    u8   jmp[3];        // 0x90 0x00 0xEB
    char oemname[8];    // "anything"
    u16  sct_size;      // 0x0200
    u8   clr_size;      // 0x40 -> 32kB clusters with 512byte sectors
    u16  sct_reserved;  // 0x20
    u8   fat_n;         // 0x02
    u16  reserved0;     // root entry count in FAT16
    u16  reserved1;     // partition size when <= 32MB
    u8   mediatype;     // 0xF8
    u16  reserved2;     // FAT size in sectors in FAT16
    u16  sct_track;     // 0x3F
    u16  sct_heads;     // 0xFF
    u32  sct_hidden;    // same as partition offset in MBR
    u32  sct_total;     // same as partition size in MBR
    u32  fat_size;      // roundup((((sct_total - sct_reserved) / clr_size) * 4) / sct_size)
    u16  flags;         // 0x00
    u16  version;       // 0x00
    u32  clr_root;      // 0x02
    u16  sct_fsinfo;    // 0x01
    u16  sct_backup;    // 0x06
    u8   reserved3[12]; 
    u8   ndrive;        // 0x80
    u8   head_cur;      // 0x00
    u8   boot_sig;      // 0x29
    u32  vol_id;        // volume id / 0x00
    char vol_label[11]; // "anything   "
    char fs_type[8];    // "FAT32   "
    u8   reserved4[420];
    u16  magic;         // 0xAA55
} __attribute__((packed)) Fat32Header;

typedef struct { // this struct is not tested enough!
    u8   jmp[3];        // 0x90 0x00 0xEB
    char oemname[8];    // "anything"
    u16  sct_size;      // 0x0200
    u8   clr_size;      // 0x20 (???) -> 16kB clusters with 512byte sectors
    u16  sct_reserved;  // 0x01
    u8   fat_n;         // 0x02
    u16  root_n;        // 0x0200
    u16  reserved0;     // partition size when <= 32MB
    u8   mediatype;     // 0xF8
    u16  fat_size;      // roundup((((sct_total - sct_reserved) / clr_size) * 2) / sct_size)
    u16  sct_track;     // 0x3F
    u16  sct_heads;     // 0xFF
    u32  sct_hidden;    // same as partition offset in MBR
    u32  sct_total;     // same as partition size in MBR
    u8   ndrive;        // 0x80
    u8   head_cur;      // 0x00
    u8   boot_sig;      // 0x29
    u32  vol_id;        // volume id / 0x00
    char vol_label[11]; // "anything   "
    char fs_type[8];    // "FAT16   "
    u8   reserved4[448];
    u16  magic;         // 0xAA55
} __attribute__((packed)) Fat16Header;

u32 ValidateMbrHeader(MbrHeader* mbr);
u32 ValidateFatHeader(void* fat);
