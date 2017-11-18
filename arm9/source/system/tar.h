#pragma once

#include "common.h"

#define USTAR_MAGIC "ustar"


// see: https://en.wikipedia.org/wiki/Tar_(computing)
// all numeric values in ASCII / octal
typedef struct {
    char fname[100];
    char fmode[8];
    char owner_id[8];
    char group_id[8];
    char fsize[12];
    char last_modified[12];
    char checksum[8];
    char ftype;
    char link_name[100];
    // ustar extension
    char magic[6]; // "ustar"
    char version[2]; // "00"
    char owner_name[32];
    char group_name[32];
    char dev_major[8];
    char dev_minor[8];
    char fname_prefix[155];
    char unused[12];
} __attribute__((packed)) TarHeader;


u32 ValidateTarHeader(void* tardata, void* tardata_end);
void* GetTarFileInfo(void* tardata, char* fname, u64* fsize, bool* is_dir);
void* NextTarEntry(void* tardata, void* tardata_end);
void* FindTarFileInfo(void* tardata, void* tardata_end, const char* fname, u64* fsize);
