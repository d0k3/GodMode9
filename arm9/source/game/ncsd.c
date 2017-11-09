#include "ncsd.h"
#include "ncch.h"

u32 ValidateNcsdHeader(NcsdHeader* header) {
    u8 zeroes[16] = { 0 };
    if ((memcmp(header->magic, "NCSD", 4) != 0) || // check magic number
        (memcmp(header->partitions_fs_type, zeroes, 8) != 0) || !header->mediaId) // prevent detection of NAND images
        return 1;
    
    u32 data_units = 0;
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = header->partitions + i;
        if ((i == 0) && !partition->size) return 1; // first content must be there
        else if (!partition->size) continue;
        if (partition->offset < data_units)
            return 1; // overlapping partitions, failed
        data_units = partition->offset + partition->size;
    }
    if (data_units > header->size)
        return 1;
     
    return 0;
}

u64 GetNcsdTrimmedSize(NcsdHeader* header) {
    u32 data_units = 0;
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = header->partitions + i;
        u32 partition_end = partition->offset + partition->size;
        if (!partition->size) continue;
        data_units = (partition_end > data_units) ? partition_end : data_units;
    }
    
    return data_units * NCSD_MEDIA_UNIT;
}

// on the fly decryptor for NCSD
u32 CryptNcsdSequential(void* data, u32 offset_data, u32 size_data, u16 crypto) {
    // warning: this will only work for sequential processing
    static NcsdHeader ncsd;
    
    // fetch ncsd header from data
    if ((offset_data == 0) && (size_data >= sizeof(NcsdHeader)))
        memcpy(&ncsd, data, sizeof(NcsdHeader));
    
    for (u32 i = 0; i < 8; i++) {
        NcchPartition* partition = ncsd.partitions + i;
        u32 offset_p = partition->offset * NCSD_MEDIA_UNIT;
        u32 size_p = partition->size * NCSD_MEDIA_UNIT;
        // check if partition in data
        if ((offset_p >= offset_data + size_data) ||
            (offset_data >= offset_p + size_p) ||
            !size_p) {
            continue; // section not in data
        }
        // determine data / offset / size
        u8* data8 = (u8*)data;
        u8* data_i = data8;
        u32 offset_i = 0;
        u32 size_i = size_p;
        if (offset_p < offset_data)
            offset_i = offset_data - offset_p;
        else data_i = data8 + (offset_p - offset_data);
        size_i = size_p - offset_i;
        if (size_i > size_data - (data_i - data8))
            size_i = size_data - (data_i - data8);
        // decrypt ncch segment
        if (CryptNcchSequential(data_i, offset_i, size_i, crypto) != 0)
            return 1;
    }
    
    return 0;
}

