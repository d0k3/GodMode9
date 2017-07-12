#include "fatmbr.h"

u32 ValidateMbrHeader(MbrHeader* mbr) {
    if (mbr->magic != FATMBR_MAGIC) return 1; // check magic
    u32 sector = 1; // check partitions
    for (u32 i = 0; i < 4; i++) {
        MbrPartitionInfo* partition = mbr->partitions + i;
        if (!partition->count && i) continue; 
        else if (!partition->count) return 1; // first partition can't be empty
        if ((partition->type != 0x1) && (partition->type != 0x4) && (partition->type != 0x6) &&
            (partition->type != 0xB) && (partition->type != 0xC) && (partition->type != 0xE))
            return 1; // bad / unknown filesystem type
        if (partition->sector < sector) return 1; // overlapping partitions
        sector = partition->sector + partition->count;
    }
    return 0;
}

u32 ValidateFatHeader(void* fat) {
    if (getle16((u8*) fat + 0x1FE) != FATMBR_MAGIC) return 1; // check magic
    Fat32Header* fat32 = (Fat32Header*) fat;
    if (strncmp(fat32->fs_type, "FAT32   ", 8) == 0)
        return 0; // is FAT32 header
    Fat16Header* fat16 = (Fat16Header*) fat;
    if ((strncmp(fat16->fs_type, "FAT16   ", 8) == 0) || 
        (strncmp(fat16->fs_type, "FAT12   ", 8) == 0) ||
        (strncmp(fat16->fs_type, "FAT     ", 8) == 0))
        return 0; // is FAT16 / FAT12 header
    if ((getle64(fat16->fs_type) == 0) && (fat16->sct_size == 0x200))
        return 0; // special case for public.sav
    return 1; // failed, not a FAT header
}
