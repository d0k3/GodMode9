#include "tar.h"


u64 ReadAsciiOctal(char* num, u32 len) {
    u64 res = 0;
    
    if ((num[len-1] != '\0') && (num[len-1] != ' '))
        return (u64) -1;
    
    for (u32 i = 0; i < (len-1); i++) {
        res <<= 3;
        if ((num[i] >= '0') && (num[i] < '8')) res |= (num[i] - '0');
        else return (u64) -1;
    }
    
    return res;
}

u32 ValidateTarHeader(void* tardata, void* tardata_end) {
    TarHeader* tar = tardata;
    
    // available space
    if ((u8*) tardata_end < (u8*) tardata + sizeof(TarHeader))
        return 1;
    
    // filename prefix is not supported here
    if (*(tar->fname_prefix)) return 1;
    
    // check filesize
    u64 fsize_max = ((u8*) tardata_end - (u8*) tardata) - sizeof(TarHeader);
    if (ReadAsciiOctal(tar->fsize, 12) > fsize_max)
        return 1;
    
    // type can only be standard file or dir
    if (tar->ftype && (tar->ftype != '0') && (tar->ftype != '5'))
        return 1;
    
    // checksum
    u8* data = (u8*) tardata;
    u64 checksum = 0;
    for (u32 i = 0; i < sizeof(TarHeader); i++)
        checksum += ((i < 148) || (i >= 156)) ? data[i] : (u64) ' ';
    if (checksum != ReadAsciiOctal(tar->checksum, 7))
        return 1;
    
    return 0;
}

void* GetTarFileInfo(void* tardata, char* fname, u64* fsize, bool* is_dir) {
    // this assumes a valid TAR header
    TarHeader* tar = tardata;
    
    if (fname) snprintf(fname, 101, "%.100s", tar->fname);
    if (fsize) *fsize = ReadAsciiOctal(tar->fsize, 12);
    if (is_dir) *is_dir = (tar->ftype == '5');
    
    return (void*) (tar + 1);
}

void* NextTarEntry(void* tardata, void* tardata_end) {
    // this assumes a valid TAR header
    TarHeader* tar = tardata;
    u8* data = (u8*) tardata;
    u64 fsize = ReadAsciiOctal(tar->fsize, 12);
    
    data += sizeof(TarHeader) + align(fsize, 512);
    if (ValidateTarHeader(data, tardata_end) != 0)
        return NULL;
    
    return data;
}

void* FindTarFileInfo(void* tardata, void* tardata_end, const char* fname, u64* fsize) {
    while (tardata && (tardata < tardata_end)) {
        TarHeader* tar = tardata;
        
        if (ValidateTarHeader(tardata, tardata_end) != 0) break;
        if ((strncasecmp(tar->fname, fname, 100) == 0) && (!tar->ftype || (tar->ftype == '0')))
            return GetTarFileInfo(tardata, NULL, fsize, NULL);
        tardata = ((u8*) tardata) + sizeof(TarHeader) + align(ReadAsciiOctal(tar->fsize, 12), 512);
    }
    
    return NULL;
}
