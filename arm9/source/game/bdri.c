#include "bdri.h"
#include "vff.h"

#define FAT_ENTRY_SIZE 2 * sizeof(u32)

#define getfatflag(uv) (((uv) & 0x80000000UL) != 0)
#define getfatindex(uv) ((uv) & 0x7FFFFFFFUL)
#define buildfatuv(index, flag) ((index) | ((flag) ? 0x80000000UL : 0))

static FIL* bdrifp;

static FRESULT BDRIRead(UINT ofs, UINT btr, void* buf) {
    if (bdrifp) {
        FRESULT res;
        UINT br;
        if ((fvx_tell(bdrifp) != ofs) &&
            (fvx_lseek(bdrifp, ofs) != FR_OK)) return FR_DENIED;
        res = fvx_read(bdrifp, buf, btr, &br);
        if ((res == FR_OK) && (br != btr)) res = FR_DENIED;
        return res;
    } else return FR_DENIED;
}

static FRESULT BDRIWrite(UINT ofs, UINT btw, const void* buf) {
    if (bdrifp) {
        FRESULT res;
        UINT bw;
        if ((fvx_tell(bdrifp) != ofs) &&
            (fvx_lseek(bdrifp, ofs) != FR_OK)) return FR_DENIED;
        res = fvx_write(bdrifp, buf, btw, &bw);
        if ((res == FR_OK) && (bw != btw)) res = FR_DENIED;
        return res;
    } else return FR_DENIED;
}

bool CheckDBMagic(const u8* pre_header, bool tickdb) {
    const TitleDBPreHeader* title = (TitleDBPreHeader*) pre_header;
    const TickDBPreHeader* tick = (TickDBPreHeader*) pre_header;
    
    return (tickdb ? ((strncmp(tick->magic, "TICK", 4) == 0) && (tick->unknown1 == 1)) : 
        ((strcmp(title->magic, "NANDIDB") == 0) || (strcmp(title->magic, "NANDTDB") == 0) ||
         (strcmp(title->magic, "TEMPIDB") == 0) || (strcmp(title->magic, "TEMPTDB") == 0))) &&
         (strcmp((tickdb ? tick->fs_header : title->fs_header).magic, "BDRI") == 0) &&
         ((tickdb ? tick->fs_header : title->fs_header).version == 0x30000);
}

// This function was taken, with slight modification, from https://3dbrew.org/wiki/Inner_FAT
static u32 GetHashBucket(const u8* tid, u32 parent_dir_index, u32 bucket_count) {
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

static u32 GetBDRIEntrySize(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, u32* size) {
    if ((fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count)) // Could be more thorough
        return 1;

    const u32 data_offset = fs_header_offset + fs_header->data_offset;
    const u32 det_offset = data_offset + fs_header->det_start_block * fs_header->data_block_size;
    const u32 fet_offset = data_offset + fs_header->fet_start_block * fs_header->data_block_size;
    
    u32 index = 0;
    TdbFileEntry file_entry;
    u64 tid_be = getbe64(title_id);
    u8* title_id_be = (u8*) &tid_be;
    
    // Read the index of the first file entry from the directory entry table
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 1;
    
    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        if (file_entry.next_sibling_index == 0)
            return 1;
        
        index = file_entry.next_sibling_index;
        
        if (BDRIRead(fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);
    
    *size = file_entry.size;

    return 0;
}

static u32 ReadBDRIEntry(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, u8* entry, const u32 expected_size) {
    if ((fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count)) // Could be more thorough
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
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 1;
    
    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        if (file_entry.next_sibling_index == 0)
            return 1;
        
        index = file_entry.next_sibling_index;
        
        if (BDRIRead(fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);
    
    if (expected_size && (file_entry.size != expected_size))
        return 1;
    
    const u32 hash_bucket = GetHashBucket(file_entry.title_id, file_entry.parent_index, fs_header->fht_bucket_count); 
    u32 index_hash = 0;
    
    if (BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != FR_OK)
        return 1;
    
    if (index != index_hash)
        return 1;
    
    index = file_entry.start_block_index + 1; // FAT entry index
    
    u32 bytes_read = 0;
    u32 fat_entry[2];
    
    while (bytes_read < file_entry.size) { // Read the full entry, walking the FAT node chain
        u32 read_start = index - 1; // Data region block index
        u32 read_count = 0;
    
        if (BDRIRead(fat_offset + index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;
        
        if ((bytes_read == 0) && !getfatflag(fat_entry[0]))
            return 1;
        
        u32 next_index = getfatindex(fat_entry[1]);
        
        if (getfatflag(fat_entry[1])) { // Multi-entry node
            if (BDRIRead(fat_offset + (index + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;
            
            if (!getfatflag(fat_entry[0]) || getfatflag(fat_entry[1]) || (getfatindex(fat_entry[0]) != index) || (getfatindex(fat_entry[0]) >= getfatindex(fat_entry[1])))
                return 1;
            
            read_count = getfatindex(fat_entry[1]) + 1 - index;
        } else { // Single-entry node
            read_count = 1;
        }
        
        index = next_index;
        
        u32 btr = min(file_entry.size - bytes_read, read_count * fs_header->data_block_size);
        if (entry && (BDRIRead(data_offset + read_start * fs_header->data_block_size, btr, entry + bytes_read) != FR_OK))
            return 1;
            
        bytes_read += btr;
    }
    
    return 0;
}

static u32 RemoveBDRIEntry(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id) {
    if ((fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count)) // Could be more thorough
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
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 1;
    
    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        previous_index = index;
        index = file_entry.next_sibling_index;
        
        if (index == 0)
            return 1;
        
        if (BDRIRead(fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);
    
    DummyFileEntry dummy_entry;
    
    // Read the 0th entry in the FET, which is always a dummy entry
    if (BDRIRead(fet_offset, sizeof(DummyFileEntry), &dummy_entry) != FR_OK)
        return 1;
    
    if (dummy_entry.max_entry_count != fs_header->max_file_count + 1)
        return 1;
    
    if ((BDRIWrite(fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &dummy_entry) != FR_OK) ||
        (BDRIWrite(fet_offset + 0x28, sizeof(u32), &index) != FR_OK) ||
        (BDRIWrite((previous_index == 0) ? det_offset + 0x2C : fet_offset + previous_index * sizeof(TdbFileEntry) + 0xC, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK))
        return 1;
    
    const u32 hash_bucket = GetHashBucket(file_entry.title_id, file_entry.parent_index, fs_header->fht_bucket_count);
    u32 index_hash = 0;
    
    if (BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != FR_OK)
        return 1;
    
    if (index_hash == index) {
        if (BDRIWrite(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &(file_entry.hash_bucket_next_index)) != FR_OK)
            return 1;
    } else {
        do {
            if (index_hash == 0) // This shouldn't happen if the entry was properly added
                break;
            
            if (BDRIRead(fet_offset + index_hash * sizeof(TdbFileEntry) + 0x28, sizeof(u32), &index_hash) != FR_OK)
                return 1;
        } while (index_hash != index);
        
        if ((index_hash != 0) && BDRIWrite(fet_offset + index_hash * sizeof(TdbFileEntry) + 0x28, sizeof(u32), &(file_entry.hash_bucket_next_index)) != FR_OK)
            return 1;
    }
    
    u32 fat_entry[2];
    
    if (BDRIRead(fat_offset, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
        return 1;
    
    if (getfatflag(fat_entry[1]) || (fat_entry[0] != 0))
        return 1;
    
    u32 next_free_index = getfatindex(fat_entry[1]), fat_index = file_entry.start_block_index + 1;
    
    if (BDRIWrite(fat_offset + sizeof(u32), sizeof(u32), &fat_index) != FR_OK)
        return 1;
    
    fat_entry[1] = fat_index;
    
    do {
        fat_index = getfatindex(fat_entry[1]);
        
        if (BDRIRead(fat_offset + FAT_ENTRY_SIZE * fat_index, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;
    } while (getfatindex(fat_entry[1]) != 0);
        
    fat_entry[1] |= next_free_index;
    
    if ((BDRIWrite(fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK) ||
        (BDRIRead(fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK))
        return 1;
        
    fat_entry[0] = buildfatuv(fat_index, false);
    
    if (BDRIWrite(fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
        return 1;
    
    return 0;
}

static u32 AddBDRIEntry(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, const u8* entry, const u32 size, bool replace) {
    if ((fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count)) // Could be more thorough
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
    bool do_replace = false;
    
    // Read the index of the first file entry from the directory entry table
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)   
        return 1;
    
    // Try to find the file entry for the tid specified
    while (file_entry.next_sibling_index != 0) {
        index = file_entry.next_sibling_index;
        max_index = max(index, max_index); 
        
        if (BDRIRead(fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 1;
        
        // If an entry for the tid already existed that is already the specified size and replace was specified, just replace the existing entry
        if (memcmp(title_id_be, file_entry.title_id, 8) == 0) {
            if (!replace || (file_entry.size != size)) return 1;
            else {
                do_replace = true;
                break;
            }
        }
    }
    
    u32 fat_entry[2];
    u32 fat_index = 0;
    
    if (!do_replace) {
        if (BDRIRead(fat_offset, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;

        if (getfatflag(fat_entry[1]) || (fat_entry[0] != 0))
            return 1;

        u32 next_fat_index = getfatindex(fat_entry[1]), node_size = 0;

        // Find contiguous free space in the FAT for the entry. Technically there could be a case of enough space existing, but not in a contiguous fasion, but this would never realistically happen
        do {
            if (next_fat_index == 0)
                return 1; // Reached the end of the free node chain without finding enough contiguous free space - this should never realistically happen

            fat_index = next_fat_index;

            if (BDRIRead(fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;

            next_fat_index = getfatindex(fat_entry[1]);

            if (getfatflag(fat_entry[1])) { // Multi-entry node
                if (BDRIRead(fat_offset + (fat_index + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                    return 1;

                if (!getfatflag(fat_entry[0]) || getfatflag(fat_entry[1]) || (getfatindex(fat_entry[0]) != fat_index) || (getfatindex(fat_entry[0]) >= getfatindex(fat_entry[1])))
                    return 1;

                node_size = getfatindex(fat_entry[1]) + 1 - fat_index;
            } else { // Single-entry node
                node_size = 1;
            }
        } while (node_size < size_blocks);

        const bool shrink_free_node = node_size > size_blocks;

        if (shrink_free_node) {
            if (BDRIRead(fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;

            if (node_size - size_blocks == 1)
                fat_entry[1] = buildfatuv(getfatindex(fat_entry[1]), false);

            if (BDRIWrite(fat_offset + (fat_index + size_blocks) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;

            if (node_size - size_blocks > 1) {
                if (BDRIRead(fat_offset + (fat_index + node_size - 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                    return 1;

                fat_entry[0] = buildfatuv(fat_index + size_blocks, true);

                if ((BDRIWrite(fat_offset + (fat_index + node_size - 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK) ||
                    (BDRIWrite(fat_offset + (fat_index + size_blocks + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK))
                    return 1;
            }
        }

        if (BDRIRead(fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;

        const u32 previous_free_index = getfatindex(fat_entry[0]), next_free_index = getfatindex(fat_entry[1]);

        fat_entry[0] = buildfatuv(0, true);
        fat_entry[1] = buildfatuv(0, size_blocks > 1);

        if (BDRIWrite(fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;

        if (size_blocks > 1) {
            fat_entry[0] = buildfatuv(fat_index, true);
            fat_entry[1] = buildfatuv(fat_index + size_blocks - 1, false);

            if ((BDRIWrite(fat_offset + (fat_index + size_blocks - 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK) ||
                (BDRIWrite(fat_offset + (fat_index + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK))
                return 1;
        }

        if (next_free_index != 0) {
            if (BDRIRead(fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;

            fat_entry[0] = buildfatuv(shrink_free_node ? fat_index + size_blocks : previous_free_index, (!shrink_free_node && (previous_free_index == 0)));

            if (BDRIWrite(fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;
        }

        if (BDRIRead(fat_offset + previous_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;

        fat_entry[1] = buildfatuv(shrink_free_node ? fat_index + size_blocks : next_free_index, getfatflag(fat_entry[1]));

        if (BDRIWrite(fat_offset + previous_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;
    } else fat_index = file_entry.start_block_index + 1;
    
    u32 bytes_written = 0, fat_index_write = fat_index;
    
    while (bytes_written < file_entry.size) { // Write the full entry, walking the FAT node chain
                                              // Can't assume contiguity here, because we might be replacing an existing entry
        u32 write_start = fat_index_write - 1; // Data region block index
        u32 write_count = 0;
    
        if (BDRIRead(fat_offset + fat_index_write * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;
        
        if ((bytes_written == 0) && !getfatflag(fat_entry[0]))
            return 1;
        
        u32 next_index = getfatindex(fat_entry[1]);
        
        if (getfatflag(fat_entry[1])) { // Multi-entry node
            if (BDRIRead(fat_offset + (fat_index_write + 1) * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
                return 1;
            
            if (!getfatflag(fat_entry[0]) || getfatflag(fat_entry[1]) || (getfatindex(fat_entry[0]) != fat_index_write) || (getfatindex(fat_entry[0]) >= getfatindex(fat_entry[1])))
                return 1;
            
            write_count = getfatindex(fat_entry[1]) + 1 - fat_index_write;
        } else { // Single-entry node
            write_count = 1;
        }
        
        fat_index_write = next_index;
        
        u32 btw = min(file_entry.size - bytes_written, write_count * fs_header->data_block_size);
        if (BDRIWrite(data_offset + write_start * fs_header->data_block_size, btw, entry + bytes_written) != FR_OK)
            return 1;
            
        bytes_written += btw;
    }
    
    if (!do_replace) {
        DummyFileEntry dummy_entry;

        // Read the 0th entry in the FET, which is always a dummy entry
        if (BDRIRead(fet_offset, sizeof(DummyFileEntry), &dummy_entry) != FR_OK) 
            return 1;

        if (dummy_entry.max_entry_count != fs_header->max_file_count + 1)
            return 1;

        if (dummy_entry.next_dummy_index == 0) { // If the 0th entry is the only dummy entry, make a new entry
            file_entry.next_sibling_index = max_index + 1;

            dummy_entry.total_entry_count++;
            if (BDRIWrite(fet_offset, sizeof(u32), &(dummy_entry.total_entry_count)) != FR_OK)
                return 1;
        } else { // If there's at least one extraneous dummy entry, replace it
            file_entry.next_sibling_index = dummy_entry.next_dummy_index;

            if ((BDRIRead(fet_offset + dummy_entry.next_dummy_index * sizeof(DummyFileEntry), sizeof(DummyFileEntry), &dummy_entry) != FR_OK) ||
                (BDRIWrite(fet_offset, sizeof(DummyFileEntry), &dummy_entry) != FR_OK))
                return 1;
        }

        if (BDRIWrite((index == 0) ? det_offset + 0x2C : fet_offset + index * sizeof(TdbFileEntry) + 0xC, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
            return 1;

        index = file_entry.next_sibling_index;

        const u32 hash_bucket = GetHashBucket(title_id_be, 1, fs_header->fht_bucket_count);
        u32 index_hash = 0;

        if ((BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != FR_OK) ||
            (BDRIWrite(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index) != FR_OK))
            return 1;

        memset(&file_entry, 0, sizeof(TdbFileEntry));
        file_entry.parent_index = 1;
        memcpy(file_entry.title_id, title_id_be, 8);
        file_entry.start_block_index = fat_index - 1;
        file_entry.size = (u64) size;
        file_entry.hash_bucket_next_index = index_hash;

        if (BDRIWrite(fet_offset + index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 1;
    }
        
    return 0;
}

static u32 GetNumBDRIEntries(const BDRIFsHeader* fs_header, const u32 fs_header_offset) {
    if ((fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count)) // Could be more thorough
        return 0;    
    
    const u32 data_offset = fs_header_offset + fs_header->data_offset;
    const u32 det_offset = data_offset + fs_header->det_start_block * fs_header->data_block_size;
    const u32 fet_offset = data_offset + fs_header->fet_start_block * fs_header->data_block_size;
    
    u32 num_entries = 0;
    TdbFileEntry file_entry;
    
    // Read the index of the first file entry from the directory entry table
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 0;
    
    while (file_entry.next_sibling_index != 0) {
        num_entries++;
        if (BDRIRead(fet_offset + file_entry.next_sibling_index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 0;
    }
    
    return num_entries;
}

static u32 ListBDRIEntryTitleIDs(const BDRIFsHeader* fs_header, const u32 fs_header_offset, u8* title_ids, u32 max_title_ids) {
    if ((fs_header->info_offset != 0x20) || (fs_header->fat_entry_count != fs_header->data_block_count))
        return 0;
    
    const u32 data_offset = fs_header_offset + fs_header->data_offset;
    const u32 det_offset = data_offset + fs_header->det_start_block * fs_header->data_block_size;
    const u32 fet_offset = data_offset + fs_header->fet_start_block * fs_header->data_block_size;
    
    u32 num_entries = 0;
    TdbFileEntry file_entry;
    
    for (u32 i = 0; i < max_title_ids; i++)
        memset(title_ids, 0, max_title_ids * 8);
    
    // Read the index of the first file entry from the directory entry table
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 1;
    
    while ((file_entry.next_sibling_index != 0) && (num_entries < max_title_ids)) {
        if (BDRIRead(fet_offset + file_entry.next_sibling_index * sizeof(TdbFileEntry), sizeof(TdbFileEntry), &file_entry) != FR_OK)
            return 1;
        memcpy(title_ids + num_entries * 8, file_entry.title_id, 8);
        num_entries++;
    }
    
    return 0;
}

u32 GetNumTitleInfoEntries(const char* path) {
    FIL file;
    TitleDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 0;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TitleDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 0;
    }
    
    u32 num = GetNumBDRIEntries(&(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader));
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return num;
}

u32 GetNumTickets(const char* path) {
    FIL file;
    TickDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 0;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TickDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 0;
    }

    u32 num = GetNumBDRIEntries(&(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader));
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return num;
}

u32 ListTitleInfoEntryTitleIDs(const char* path, u8* title_ids, u32 max_title_ids) {
    FIL file;
    TitleDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TitleDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (ListBDRIEntryTitleIDs(&(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_ids, max_title_ids) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}

u32 ListTicketTitleIDs(const char* path, u8* title_ids, u32 max_title_ids) {
    FIL file;
    TickDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TickDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (ListBDRIEntryTitleIDs(&(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_ids, max_title_ids) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}

u32 ReadTitleInfoEntryFromDB(const char* path, const u8* title_id, TitleInfoEntry* tie) {
    FIL file;
    TitleDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TitleDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (ReadBDRIEntry(&(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_id, (u8*) tie,
            sizeof(TitleInfoEntry)) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}

u32 ReadTicketFromDB(const char* path, const u8* title_id, Ticket** ticket)
{
    FIL file;
    TickDBPreHeader pre_header;
    TicketEntry* te = NULL;
    u32 entry_size;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TickDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (GetBDRIEntrySize(&(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id, &entry_size) != 0) ||
        entry_size < sizeof(TicketEntry) + 0x14 || 
        (te = (TicketEntry*)malloc(entry_size), te == NULL) ||
        (ReadBDRIEntry(&(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id, (u8*) te,
            sizeof(TicketEntry)) != 0)) {
        free(te); // if allocated
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    
    if (te->ticket_size != GetTicketSize(&te->ticket)) {
        free(te);
        return 1;
    }
    
    if (ticket) {
        u32 size = te->ticket_size;
        memmove(te, &te->ticket, size); // recycle this memory, instead of allocating another
        Ticket* tik = realloc(te, size);
        if(!tik) tik = (Ticket*)te;
        *ticket = tik;
        return 0;
    }
    
    free(te);
    return 0;
}

u32 RemoveTitleInfoEntryFromDB(const char* path, const u8* title_id) {
    FIL file;
    TitleDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TitleDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (RemoveBDRIEntry(&(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_id) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}

u32 RemoveTicketFromDB(const char* path, const u8* title_id) {
    FIL file;
    TickDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TickDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (RemoveBDRIEntry(&(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(&file);
    bdrifp = NULL;
    return 0;
}

u32 AddTitleInfoEntryToDB(const char* path, const u8* title_id, const TitleInfoEntry* tie, bool replace) {
    FIL file;
    TitleDBPreHeader pre_header;
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TitleDBPreHeader), &pre_header) != FR_OK) || 
        !CheckDBMagic((u8*) &pre_header, false) ||
        (AddBDRIEntry(&(pre_header.fs_header), sizeof(TitleDBPreHeader) - sizeof(BDRIFsHeader), title_id,
            (const u8*) tie, sizeof(TitleInfoEntry), replace) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}

u32 AddTicketToDB(const char* path, const u8* title_id, const Ticket* ticket, bool replace) {
    FIL file;
    TickDBPreHeader pre_header;
    u32 entry_size = sizeof(TicketEntry) + GetTicketContentIndexSize(ticket);
    
    TicketEntry* te = (TicketEntry*)malloc(entry_size);
    if (!te) {
        return 1;
    }

    te->unknown = 1;
    te->ticket_size = GetTicketSize(ticket);
    memcpy(&te->ticket, ticket, te->ticket_size);
    
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) {
        free(te);
        return 1;
    }
    
    bdrifp = &file;
    
    if ((BDRIRead(0, sizeof(TickDBPreHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (AddBDRIEntry(&(pre_header.fs_header), sizeof(TickDBPreHeader) - sizeof(BDRIFsHeader), title_id,
            (const u8*) te, entry_size, replace) != 0)) {
        free(te);
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }
    
    free(te);
    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}