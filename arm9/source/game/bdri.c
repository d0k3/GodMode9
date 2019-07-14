#include "bdri.h"
#include "disadiff.h"
#include "ui.h"

#define FAT_ENTRY_SIZE 2 * sizeof(u32)

static inline bool getflag (u32 uv) { return (uv & 0x80000000UL) != 0; }
static inline u32 getindex(u32 uv) { return uv & 0x7FFFFFFFUL; }
static inline u32 builduv(u32 index, bool flag) { return index | (flag ? 0x80000000UL : 0); }

bool CheckDBMagic(const u8* pre_header, bool tickdb)
{
    const TitleDBPreHeader* title = (TitleDBPreHeader*) pre_header;
    const TickDBPreHeader* tick = (TickDBPreHeader*) pre_header;
    
    return (tickdb ? ((strncmp(tick->magic, "TICK", 4) == 0) && (tick->unknown1 == 1)) : 
        ((strcmp(title->magic, "NANDIDB") == 0) || (strcmp(title->magic, "NANDTDB") == 0) ||
         (strcmp(title->magic, "TEMPIDB") == 0) || (strcmp(title->magic, "TEMPTDB") == 0))) &&
         (strcmp((tickdb ? tick->fs_header : title->fs_header).magic, "BDRI") == 0) &&
         ((tickdb ? tick->fs_header : title->fs_header).version == 0x30000);
}

// This function was taken, with slight modification, from https://3dbrew.org/wiki/Inner_FAT
static u32 GetHashBucket(const u8* tid, u32 parent_dir_index, u32 bucket_count)
{
    u32 hash = parent_dir_index ^ 0x091A2B3C;
    for (u8 i = 0; i < 2; ++i) {
        hash = (hash >> 1) | (hash << 31);
        hash ^= (u32)tid[i * 4];
        hash ^= (u32)tid[i * 4 + 1] << 8;
        hash ^= (u32)tid[i * 4 + 2] << 16;
        hash ^= (u32)tid[i * 4 + 3] << 24;
    }
    return hash % bucket_count;
}

static u32 ReadBDRIEntry(const DisaDiffRWInfo* info, const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, u8* entry, const u32 expected_size)
{
    if ((fs_header->image_size != info->size_ivfc_lvl4 / fs_header->image_block_size) || 
        (fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count))
        return 1;
    
    const u32 data_offset = fs_header_offset + fs_header->data_offset;
    const u32 det_offset = data_offset + fs_header->det_start_block * fs_header->data_block_size;
    const u32 fet_offset = data_offset + fs_header->fet_start_block * fs_header->data_block_size;
    const u32 fht_offset = fs_header_offset + fs_header->fht_offset;
    const u32 fat_offset = fs_header_offset + fs_header->fat_offset;
    
    u32 index = 0;
    TdbFileEntry file_entry;
    u64 tid_be = getbe64(title_id);
    u8* title_id_be = (u8*) &tid_be;
    
    // Read the index of the first file entry from the directory entry table
    if (ReadDisaDiffIvfcLvl4(NULL, info, det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != sizeof(u32))
        return 1;
    
    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        if (file_entry.next_sibling_index == 0)
            return 1;
        
        index = file_entry.next_sibling_index;
        
        if (ReadDisaDiffIvfcLvl4(NULL, info, fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != sizeof(TdbFileEntry))
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);
    
    if (expected_size && (file_entry.size != expected_size))
        return 1;
    
    const u32 hash_bucket = GetHashBucket(file_entry.title_id, file_entry.parent_index, fs_header->fht_bucket_count); 
    u32 index_hash = 0;
    
    if (ReadDisaDiffIvfcLvl4(NULL, info, fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != sizeof(u32))
        return 1;
    
    if (index != index_hash)
        return 1;
    
    index = file_entry.start_block_index + 1; // FAT entry index
    
    u32 bytes_read = 0;
    u32 fat_entry[2];
    
    while (bytes_read < file_entry.size) { // Read the full file, walking the FAT node chain
        u32 read_start = index - 1; // Data region block index
        u32 read_count = 0;
    
        if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
        
        if ((bytes_read == 0) && !getflag(fat_entry[0]))
            return 1;
        
        u32 next_index = getindex(fat_entry[1]);
        
        if (getflag(fat_entry[1])) { // Multi-entry node
            if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + (index + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
                return 1;
            
            if (!getflag(fat_entry[0]) || getflag(fat_entry[1]) || (getindex(fat_entry[0]) != index) || (getindex(fat_entry[0]) >= getindex(fat_entry[1])))
                return 1;
            
            read_count = getindex(fat_entry[1]) + 1 - index;
        } else { // Single-entry node
            read_count = 1;
        }
        
        index = next_index;
        
        u32 btr = min(file_entry.size - bytes_read, read_count * fs_header->data_block_size);
        if (entry && (ReadDisaDiffIvfcLvl4(NULL, info, data_offset + read_start * fs_header->data_block_size, btr, entry + bytes_read) != btr))
            return 1;
            
        bytes_read += btr;
    }
    
    return 0;
}

static u32 RemoveBDRIEntry(const DisaDiffRWInfo* info, const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id)
{
    if ((fs_header->image_size != info->size_ivfc_lvl4 / fs_header->image_block_size) || 
        (fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count))
        return 1;
    
    const u32 data_offset = fs_header_offset + fs_header->data_offset;
    const u32 det_offset = data_offset + fs_header->det_start_block * fs_header->data_block_size;
    const u32 fet_offset = data_offset + fs_header->fet_start_block * fs_header->data_block_size;
    const u32 fht_offset = fs_header_offset + fs_header->fht_offset;
    const u32 fat_offset = fs_header_offset + fs_header->fat_offset;
    
    u32 index = 0, previous_index = 0;
    TdbFileEntry file_entry;
    u64 tid_be = getbe64(title_id);
    u8* title_id_be = (u8*) &tid_be;
    
    // Read the index of the first file entry from the directory entry table
    if (ReadDisaDiffIvfcLvl4(NULL, info, det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != sizeof(u32))
        return 1;
    
    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        previous_index = index;
        index = file_entry.next_sibling_index;
        
        if (index == 0)
            return 1;
        
        if (ReadDisaDiffIvfcLvl4(NULL, info, fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != sizeof(TdbFileEntry))
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);
    
    DummyFileEntry dummy_entry;
    
    // Read the 0th entry in the FET, which is always a dummy entry
    if (ReadDisaDiffIvfcLvl4(NULL, info, fet_offset, sizeof(DummyFileEntry), &dummy_entry) != sizeof(DummyFileEntry))
        return 1;
    
    if (dummy_entry.max_entry_count != fs_header->max_file_count + 1)
        return 1;
    
    if ((WriteDisaDiffIvfcLvl4(NULL, info, fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &dummy_entry) != sizeof(DummyFileEntry)) ||
        (WriteDisaDiffIvfcLvl4(NULL, info, fet_offset + 0x28, sizeof(u32), &index) != sizeof(u32)) ||
        (WriteDisaDiffIvfcLvl4(NULL, info, (previous_index == 0) ? det_offset + 0x2C : fet_offset + previous_index * sizeof(TdbFileEntry) + 0xC,
            sizeof(u32), &(file_entry.next_sibling_index)) != sizeof(u32)))
        return 1;
    
    const u32 hash_bucket = GetHashBucket(file_entry.title_id, file_entry.parent_index, fs_header->fht_bucket_count);
    u32 index_hash = 0;
    
    if (ReadDisaDiffIvfcLvl4(NULL, info, fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != sizeof(u32))
        return 1;
    
    if (index_hash == index) {
        if (WriteDisaDiffIvfcLvl4(NULL, info, fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &(file_entry.hash_bucket_next_index)) != sizeof(u32))
            return 1;
    } else {
        do {
            if (index_hash == 0) // This shouldn't happen if the entry was properly added
                break;
            
            if (ReadDisaDiffIvfcLvl4(NULL, info, fet_offset + index_hash * sizeof(TdbFileEntry) + 0x28, sizeof(u32), &index_hash) != sizeof(u32))
                return 1;
        } while (index_hash != index);
        
        if ((index_hash != 0) && WriteDisaDiffIvfcLvl4(NULL, info, fet_offset + index_hash * sizeof(TdbFileEntry) + 0x28, sizeof(u32), &(file_entry.hash_bucket_next_index)) != sizeof(u32))
            return 1;
    }
    
    u32 fat_entry[2];
    
    if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    if (getflag(fat_entry[1]) || (fat_entry[0] != 0))
        return 1;
    
    u32 next_free_index = getindex(fat_entry[1]), fat_index = file_entry.start_block_index + 1;
    
    if (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + sizeof(u32), sizeof(u32), &fat_index) != sizeof(u32))
        return 1;
    
    fat_entry[1] = fat_index;
    
    do {
        fat_index = getindex(fat_entry[1]);
        
        if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + FAT_ENTRY_SIZE * fat_index, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
    } while (getindex(fat_entry[1]) != 0);
        
    fat_entry[1] |= next_free_index;
    
    if ((WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE) ||
        (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE))
        return 1;
        
    fat_entry[0] = builduv(fat_index, false);
    
    if (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    return 0;
}

static u32 AddBDRIEntry(const DisaDiffRWInfo* info, const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, const u8* entry, const u32 size)
{
    if ((fs_header->image_size != info->size_ivfc_lvl4 / fs_header->image_block_size) || 
        (fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count))
        return 1;
    
    if (!entry || !size)
        return 1;
    
    const u32 data_offset = fs_header_offset + fs_header->data_offset;
    const u32 det_offset = data_offset + fs_header->det_start_block * fs_header->data_block_size;
    const u32 fet_offset = data_offset + fs_header->fet_start_block * fs_header->data_block_size;
    const u32 fht_offset = fs_header_offset + fs_header->fht_offset;
    const u32 fat_offset = fs_header_offset + fs_header->fat_offset;
    const u32 size_blocks = (size / fs_header->data_block_size) + (((size % fs_header->data_block_size) == 0) ? 0 : 1);
    
    u32 index = 0, max_index = 0;
    TdbFileEntry file_entry;
    u64 tid_be = getbe64(title_id);
    u8* title_id_be = (u8*) &tid_be;
    
    // Read the index of the first file entry from the directory entry table
    if (ReadDisaDiffIvfcLvl4(NULL, info, det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != sizeof(u32))   
        return 1;
    
    // Try to find the file entry for the tid specified, fail if it already exists
    while (file_entry.next_sibling_index != 0) {
        index = file_entry.next_sibling_index;
        max_index = max(index, max_index);
        
        if (memcmp(title_id_be, file_entry.title_id, 8) == 0)
            return 1;
        
        if (ReadDisaDiffIvfcLvl4(NULL, info, fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != sizeof(TdbFileEntry))
            return 1;
    }
    
    u32 fat_entry[2];
    
    if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    if (getflag(fat_entry[1]) || (fat_entry[0] != 0))
        return 1;
    
    u32 next_fat_index = getindex(fat_entry[1]), node_size = 0, fat_index = 0;
    
    // Find contiguous free space in the FAT for the entry. Technically there could be a case of enough space existing, but not in a contiguous fasion, but this would never realistically happen
    do {
        if (next_fat_index == 0)
            return 1; // Reached the end of the free node chain without finding enough contiguous free space - this should never realistically happen
        
        fat_index = next_fat_index;
        
        if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
        
        next_fat_index = getindex(fat_entry[1]);
        
        if (getflag(fat_entry[1])) { // Multi-entry node
            if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
                return 1;
            
            if (!getflag(fat_entry[0]) || getflag(fat_entry[1]) || (getindex(fat_entry[0]) != fat_index) || (getindex(fat_entry[0]) >= getindex(fat_entry[1])))
                return 1;
            
            node_size = getindex(fat_entry[1]) + 1 - fat_index;
        } else { // Single-entry node
            node_size = 1;
        }
    } while (node_size < size_blocks);
    
    const bool shrink_free_node = node_size > size_blocks;
    
    if (shrink_free_node) {
        if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
        
        if (node_size - size_blocks == 1)
            fat_entry[1] = builduv(getindex(fat_entry[1]), false);
        
        if (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + size_blocks) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
        
        if (node_size - size_blocks > 1) {
            if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + node_size - 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
                return 1;
            
            fat_entry[0] = builduv(fat_index + size_blocks, true);
            
            if ((WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + node_size - 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE) ||
                (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + size_blocks + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE))
                return 1;
        }
    }
    
    if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    const u32 previous_free_index = getindex(fat_entry[0]), next_free_index = getindex(fat_entry[1]);
    
    fat_entry[0] = builduv(0, true);
    fat_entry[1] = builduv(0, size_blocks > 1);
    
    if (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    if (size_blocks > 1) {
        fat_entry[0] = builduv(fat_index, true);
        fat_entry[1] = builduv(fat_index + size_blocks - 1, false);
        
        if ((WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + size_blocks - 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE) ||
            (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + (fat_index + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE))
            return 1;
    }
    
    if (next_free_index != 0) {
        if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
        
        fat_entry[0] = builduv(shrink_free_node ? fat_index + size_blocks : previous_free_index, (!shrink_free_node && (previous_free_index == 0)));
        
        if (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
            return 1;
    }
    
    if (ReadDisaDiffIvfcLvl4(NULL, info, fat_offset + previous_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    fat_entry[1] = builduv(shrink_free_node ? fat_index + size_blocks : next_free_index, getflag(fat_entry[1]));
    
    if (WriteDisaDiffIvfcLvl4(NULL, info, fat_offset + previous_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FAT_ENTRY_SIZE)
        return 1;
    
    // Actual writing of the entry data
    if (WriteDisaDiffIvfcLvl4(NULL, info, data_offset + (fat_index - 1) * fs_header->data_block_size, size, entry) != size)
        return 1;
    
    DummyFileEntry dummy_entry;
    
    // Read the 0th entry in the FET, which is always a dummy entry
    if (ReadDisaDiffIvfcLvl4(NULL, info, fet_offset, sizeof(DummyFileEntry), &dummy_entry) != sizeof(DummyFileEntry)) 
        return 1;
    
    if (dummy_entry.max_entry_count != fs_header->max_file_count + 1)
        return 1;
    
    if (dummy_entry.next_dummy_index == 0) { // If the 0th entry is the only dummy entry, make a new entry
        file_entry.next_sibling_index = max_index + 1;
        
        dummy_entry.total_entry_count++;
        if (WriteDisaDiffIvfcLvl4(NULL, info, fet_offset, sizeof(u32), &(dummy_entry.total_entry_count)) != sizeof(u32))
            return 1;
    } else { // If there's at least one extraneous dummy entry, replace it
        file_entry.next_sibling_index = dummy_entry.next_dummy_index;
    
        if ((ReadDisaDiffIvfcLvl4(NULL, info, fet_offset + dummy_entry.next_dummy_index * sizeof(DummyFileEntry), sizeof(DummyFileEntry), &dummy_entry) != sizeof(DummyFileEntry)) ||
            (WriteDisaDiffIvfcLvl4(NULL, info, fet_offset, sizeof(DummyFileEntry), &dummy_entry) != sizeof(DummyFileEntry)))
            return 1;
    }
    
    if (WriteDisaDiffIvfcLvl4(NULL, info, (index == 0) ? det_offset + 0x2C : fet_offset + index * sizeof(TdbFileEntry) + 0xC, sizeof(u32), &(file_entry.next_sibling_index)) != sizeof(u32))
        return 1;
    
    index = file_entry.next_sibling_index;
    
    const u32 hash_bucket = GetHashBucket(title_id_be, 1, fs_header->fht_bucket_count);
    u32 index_hash = 0;
    
    if ((ReadDisaDiffIvfcLvl4(NULL, info, fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != sizeof(u32)) ||
        (WriteDisaDiffIvfcLvl4(NULL, info, fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index) != sizeof(u32)))
        return 1;
    
    memset(&file_entry, 0, sizeof(TdbFileEntry));
    file_entry.parent_index = 1;
    memcpy(file_entry.title_id, title_id_be, 8);
    file_entry.start_block_index = fat_index - 1;
    file_entry.size = (u64) size;
    file_entry.hash_bucket_next_index = index_hash;
    
    if (WriteDisaDiffIvfcLvl4(NULL, info, fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != sizeof(TdbFileEntry))
        return 1;
    
    return 0;
}

u32 ReadTitleInfoEntryFromDB(const char* path, const u8* title_id, TitleInfoEntry* tie)
{
    FIL file;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    SetDisaDiffFile(&file);
    
    const DisaDiffRWInfo info;
    u8* lvl2_cache = NULL;
    if ((GetDisaDiffRWInfo(NULL, (DisaDiffRWInfo*) &info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, lvl2_cache, info.size_dpfs_lvl2) != 0)) {
        if (lvl2_cache) free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    TitleDBPreHeader pre_header;
    
    if (ReadDisaDiffIvfcLvl4(NULL, &info, 0, sizeof(TitleDBPreHeader), &pre_header) != sizeof(TitleDBPreHeader)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (!CheckDBMagic((u8*) &pre_header, false)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (ReadBDRIEntry(&info, &(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_id, (u8*) tie, sizeof(TitleInfoEntry)) != 0) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    SetDisaDiffFile(NULL);
    fvx_close(&file);
    free(lvl2_cache);
    return 0;
}

u32 ReadTicketFromDB(const char* path, const u8* title_id, Ticket* ticket)
{
    FIL file;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    SetDisaDiffFile(&file);
    
    const DisaDiffRWInfo info;
    u8* lvl2_cache = NULL;
    if ((GetDisaDiffRWInfo(NULL, (DisaDiffRWInfo*) &info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, lvl2_cache, info.size_dpfs_lvl2) != 0)) {
        if (lvl2_cache) free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    TicketEntry te;
    TickDBPreHeader pre_header;
    
    if (ReadDisaDiffIvfcLvl4(NULL, &info, 0, sizeof(TickDBPreHeader), &pre_header) != sizeof(TickDBPreHeader)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (!CheckDBMagic((u8*) &pre_header, true)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (ReadBDRIEntry(&info, &(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id, (u8*) &te, sizeof(TicketEntry)) != 0) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    SetDisaDiffFile(NULL);
    fvx_close(&file);
    free(lvl2_cache);
    
    if (te.unknown != 1)
        ShowPrompt(false, "Warning: ticket entry unknown u32 was 0x%X", te.unknown);
    
    if (te.ticket_size != sizeof(Ticket))
        return 1;
    
    *ticket = te.ticket;
    
    return 0;
}

u32 RemoveTitleInfoEntryFromDB(const char* path, const u8* title_id)
{
    FIL file;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    SetDisaDiffFile(&file);
    
    const DisaDiffRWInfo info;
    u8* lvl2_cache = NULL;
    if ((GetDisaDiffRWInfo(NULL, (DisaDiffRWInfo*) &info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, lvl2_cache, info.size_dpfs_lvl2) != 0)) {
        if (lvl2_cache) free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    const TitleDBPreHeader pre_header;
    
    if (ReadDisaDiffIvfcLvl4(NULL, &info, 0, sizeof(TitleDBPreHeader), (TitleDBPreHeader*) &pre_header) != sizeof(TitleDBPreHeader)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (!CheckDBMagic((const u8*) &pre_header, false)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (RemoveBDRIEntry(&info, &(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_id) != 0) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    SetDisaDiffFile(NULL);
    fvx_close(&file);
    free(lvl2_cache);
    return 0;
}

u32 RemoveTicketFromDB(const char* path, const u8* title_id)
{
    FIL file;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    SetDisaDiffFile(&file);
    
    const DisaDiffRWInfo info;
    u8* lvl2_cache = NULL;
    if ((GetDisaDiffRWInfo(NULL, (DisaDiffRWInfo*) &info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, lvl2_cache, info.size_dpfs_lvl2) != 0)) {
        if (lvl2_cache) free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    TickDBPreHeader pre_header;
    
    if (ReadDisaDiffIvfcLvl4(NULL, &info, 0, sizeof(TickDBPreHeader), &pre_header) != sizeof(TickDBPreHeader)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (!CheckDBMagic((u8*) &pre_header, true)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (RemoveBDRIEntry(&info, &(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id) != 0) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    SetDisaDiffFile(NULL);
    fvx_close(&file);
    free(lvl2_cache);
    return 0;
}

u32 AddTitleInfoEntryToDB(const char* path, const u8* title_id, const TitleInfoEntry* tie)
{
    FIL file;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    SetDisaDiffFile(&file);
    
    const DisaDiffRWInfo info;
    u8* lvl2_cache = NULL;
    if ((GetDisaDiffRWInfo(NULL, (DisaDiffRWInfo*) &info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, lvl2_cache, info.size_dpfs_lvl2) != 0)) {
        if (lvl2_cache) free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    TitleDBPreHeader pre_header;
    
    if (ReadDisaDiffIvfcLvl4(NULL, &info, 0, sizeof(TitleDBPreHeader), &pre_header) != sizeof(TitleDBPreHeader)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (!CheckDBMagic((u8*) &pre_header, false)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (AddBDRIEntry(&info, &(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_id, (const u8*) tie, sizeof(TitleInfoEntry)) != 0) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    SetDisaDiffFile(NULL);
    fvx_close(&file);
    free(lvl2_cache);
    return 0;
}

u32 AddTicketToDB(const char* path, const u8* title_id, const Ticket* ticket)
{
    FIL file;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    SetDisaDiffFile(&file);
    
    const DisaDiffRWInfo info;
    u8* lvl2_cache = NULL;
    if ((GetDisaDiffRWInfo(NULL, (DisaDiffRWInfo*) &info, false) != 0) ||
        !(lvl2_cache = (u8*) malloc(info.size_dpfs_lvl2)) ||
        (BuildDisaDiffDpfsLvl2Cache(NULL, &info, lvl2_cache, info.size_dpfs_lvl2) != 0)) {
        if (lvl2_cache) free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    TicketEntry te;
    te.unknown = 1;
    te.ticket_size = sizeof(Ticket);
    te.ticket = *ticket;
    
    TickDBPreHeader pre_header;
    
    if (ReadDisaDiffIvfcLvl4(NULL, &info, 0, sizeof(TickDBPreHeader), &pre_header) != sizeof(TickDBPreHeader)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (!CheckDBMagic((u8*) &pre_header, true)) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    if (AddBDRIEntry(&info, &(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id, (const u8*) &te, sizeof(TicketEntry)) != 0) {
        free(lvl2_cache);
        SetDisaDiffFile(NULL);
        fvx_close(&file);
        return 1;
    }
    
    SetDisaDiffFile(NULL);
    fvx_close(&file);
    free(lvl2_cache);
    return 0;
}