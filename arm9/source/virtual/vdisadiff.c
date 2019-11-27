#include "vdisadiff.h"
#include "disadiff.h"
#include "common.h"
#include "image.h"
#include "vtickdb.h" // So we can mount a file as vdisadiff and vtickdb simeltaneously

#define VFLAG_PARTITION_B (1 << 31)

#define MAX_IVFC_RANGES 1024

typedef struct {
    u32 offset;
    u32 size;
} PACKED_STRUCT DisaDiffIvfcRange;

typedef struct {
    u32 n_ivfc_ranges;
    DisaDiffIvfcRange ivfc_lvl4_ranges[MAX_IVFC_RANGES];
    DisaDiffRWInfo rw_info;
} PACKED_STRUCT VDisaDiffPartitionInfo;

static VDisaDiffPartitionInfo* partitionA_info = NULL;
static VDisaDiffPartitionInfo* partitionB_info = NULL;

static void AlignDisaDiffIvfcRange(DisaDiffIvfcRange* range, u32 log_block_size) {
    u32 end_offset = align(range->offset + range->size, 1 << log_block_size);
    range->offset = (range->offset >> log_block_size) << log_block_size;
    range->size = end_offset - range->offset;
}

static u32 MergeDisaDiffIvfcRange(DisaDiffIvfcRange new_range, DisaDiffIvfcRange* ranges, u32* n_ranges) {
    const u32 new_end_offset = new_range.offset + new_range.size;
    bool add_range = true;
    
    if (*n_ranges > MAX_IVFC_RANGES)
        return 1;
    
    for (u32 i = 0; i < *n_ranges; i++) {
        u32 end_offset = ranges[i].offset + ranges[i].size;
        
        if (new_range.offset > ranges[i].offset) {
            if (end_offset >= new_range.offset) {
                add_range = false;
                
                if (end_offset < new_end_offset) {
                    ranges[i].size += new_end_offset - end_offset;
                }
                
                break;
            }
            
            continue;
        } else if (ranges[i].offset > new_range.offset) {
            if (new_end_offset >= ranges[i].offset) {
                add_range = false;
                
                if (new_end_offset < end_offset) {
                    ranges[i].offset = new_range.offset;
                    ranges[i].size = new_range.size + end_offset - new_end_offset;
                } else {
                    ranges[i] = new_range;
                }
                
                break;
            }
            
            continue;
        } else {
            add_range = false;
            ranges[i].size = max(new_range.size, ranges[i].size);
            break;
        }
    }
    
    if (add_range) {
        if (*n_ranges == MAX_IVFC_RANGES - 1)
            return 1;
        
        ranges[(*n_ranges)++] = new_range;
    }
    
    return 0;
}

static u32 FixVDisaDiffIvfcHashChain(bool partitionB) {
    VDisaDiffPartitionInfo* info = partitionB ? partitionB_info : partitionA_info;
    if (!info) return 1;
    
    if (info->n_ivfc_ranges == 0)
        return 0;
    
    DisaDiffIvfcRange* ivfc_range_buffer = malloc(sizeof(DisaDiffIvfcRange) * MAX_IVFC_RANGES);
    if (!ivfc_range_buffer)
        return 1;
    
    u32 n_ivfc_ranges = 0;
    
    for (u32 i = 0; i < info->n_ivfc_ranges; i++) {
        if (MergeDisaDiffIvfcRange(info->ivfc_lvl4_ranges[i], ivfc_range_buffer, &n_ivfc_ranges) != 0)
            return 1;
    }
    
    for (u32 level = 4; level > 0; level--) {
        u32 next_n_ivfc_ranges = 0;
        DisaDiffIvfcRange* ivfc_ranges = (level % 2) ? info->ivfc_lvl4_ranges : ivfc_range_buffer;
        DisaDiffIvfcRange* next_ivfc_ranges = (level % 2) ? ivfc_range_buffer : info->ivfc_lvl4_ranges;
        
        for (u32 j = 0; j < n_ivfc_ranges; j++) {
            DisaDiffIvfcRange next_range;
            
            if (FixDisaDiffIvfcLevel(&(info->rw_info), level, ivfc_ranges[j].offset, ivfc_ranges[j].size, &(next_range.offset), &(next_range.size)) != 0)
                return 1;
            
            if (next_ivfc_ranges) {
                AlignDisaDiffIvfcRange(&next_range, (&(info->rw_info.log_ivfc_lvl1))[level - 2]);
                if (MergeDisaDiffIvfcRange(next_range, next_ivfc_ranges, &next_n_ivfc_ranges) != 0)
                    return 1;
            }
        }
        n_ivfc_ranges = next_n_ivfc_ranges;
    }
    
    free(ivfc_range_buffer);
    
    if (FixDisaDiffIvfcLevel(&(info->rw_info), 0, 0, 0, NULL, NULL) != 0)
        return 1;
    
    info->n_ivfc_ranges = 0;
    return 0;
}

void DeinitVDisaDiffDrive(void) {
    if (partitionA_info) {
        FixVDisaDiffIvfcHashChain(false);
        if (partitionA_info->rw_info.dpfs_lvl2_cache)
            free(partitionA_info->rw_info.dpfs_lvl2_cache);
        free(partitionA_info);
        partitionA_info = NULL;
    }
    
    if (partitionB_info) {
        FixVDisaDiffIvfcHashChain(true);
        if (partitionB_info->rw_info.dpfs_lvl2_cache)
            free(partitionB_info->rw_info.dpfs_lvl2_cache);
        free(partitionB_info);
        partitionB_info = NULL;
    }
}

u64 InitVDisaDiffDrive(void) {
    DisaDiffRWInfo info;
    u64 type;
    
    DeinitVDisaDiffDrive();
    
    if (!(type = (GetMountState() & (SYS_DISA | SYS_DIFF))))
        return 0;
    
    if ((GetDisaDiffRWInfo(NULL, &info, false) != 0) ||
        (!(info.dpfs_lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, info.dpfs_lvl2_cache, info.size_dpfs_lvl2) != 0))) {
        free(info.dpfs_lvl2_cache);
        return 0;
   }
    
    if (!(partitionA_info = malloc(sizeof(VDisaDiffPartitionInfo)))) {
        free(info.dpfs_lvl2_cache);
        partitionA_info = NULL;
        return 0;
    }
    
    memset(partitionA_info, 0, sizeof(VDisaDiffPartitionInfo));
    partitionA_info->rw_info = info;
    
    if ((type & SYS_DISA) && (GetDisaDiffRWInfo(NULL, &info, true) == 0)) {
        if (!(info.dpfs_lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
            (BuildDisaDiffDpfsLvl2Cache(NULL, &info, info.dpfs_lvl2_cache, info.size_dpfs_lvl2) != 0)) {
            if (partitionA_info->rw_info.dpfs_lvl2_cache) free(partitionA_info->rw_info.dpfs_lvl2_cache);
            free(partitionA_info);
            partitionA_info = NULL;
            return 0;
        }
        
        if (!(partitionB_info = malloc(sizeof(VDisaDiffPartitionInfo)))) {
            if (info.dpfs_lvl2_cache) free(info.dpfs_lvl2_cache);
            if (partitionA_info->rw_info.dpfs_lvl2_cache) free(partitionA_info->rw_info.dpfs_lvl2_cache);
            free(partitionA_info);
            partitionA_info = NULL;
            partitionB_info = NULL;
            return 0;
        }
        
        memset(partitionB_info, 0, sizeof(VDisaDiffPartitionInfo));
        partitionB_info->rw_info = info;
    }
    
    InitVTickDbDrive();
    
    return type;
}

u64 CheckVDisaDiffDrive(void) {
    u64 type = GetMountState() & (SYS_DISA | SYS_DIFF);
    
    return (type && partitionA_info) ? type : 0;
}

// Can be very lazy here because there are only two files that can appear
bool ReadVDisaDiffDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (++(vdir->index) > 1)
        return false;
    
    VDisaDiffPartitionInfo* info = vdir->index ? partitionB_info : partitionA_info;
    
    if (!info)
        return false;
    
    memset(vfile, 0, sizeof(VirtualFile));
    snprintf(vfile->name, 32, "partition%c.bin", vdir->index ? 'B' : 'A');
    vfile->size = info->rw_info.size_ivfc_lvl4;
    if (vdir->index) vfile->flags |= VFLAG_PARTITION_B;
    
    return true;
}

int ReadVDisaDiffFile(const VirtualFile* vfile, void* buffer, u64 offset, u64 count) {
    VDisaDiffPartitionInfo* info = (vfile->flags & VFLAG_PARTITION_B) ? partitionB_info : partitionA_info;
    if (!info) return 1;
    u32 ret = ReadDisaDiffIvfcLvl4(NULL, &(info->rw_info), offset, count, buffer);
    //ShowPrompt(false, "Ret: %X\nCount: %X", ret, count);
    return (ret == count) ? 0 : 1;
}

int WriteVDisaDiffFile(const VirtualFile* vfile, const void* buffer, u64 offset, u64 count) {
    VDisaDiffPartitionInfo* info = (vfile->flags & VFLAG_PARTITION_B) ? partitionB_info : partitionA_info;
    if (!info) return 1;
    
    if (WriteDisaDiffIvfcLvl4(NULL, &(info->rw_info), offset, count, buffer) != count) return 1;
    
    DisaDiffIvfcRange range;
    range.offset = offset;
    range.size = count;
    AlignDisaDiffIvfcRange(&range, info->rw_info.log_ivfc_lvl4);
    if ((MergeDisaDiffIvfcRange(range, info->ivfc_lvl4_ranges, &(info->n_ivfc_ranges)) != 0) &&
        ((FixVDisaDiffIvfcHashChain(vfile->flags & VFLAG_PARTITION_B) != 0) ||
        (MergeDisaDiffIvfcRange(range, info->ivfc_lvl4_ranges, &(info->n_ivfc_ranges)) != 0)))
        return 1;
        
    return 0;
}
