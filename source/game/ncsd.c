#include "ncsd.h"

u32 ValidateNcsdHeader(NcsdHeader* header) {
    u8 zeroes[16] = { 0 };
    if ((memcmp(header->magic, "NCSD", 4) != 0) || // check magic number
        (memcmp(header->partitions_fs_type, zeroes, 8) != 0)) // prevent detection of NAND images
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
