#include "fsinit.h"
#include "fsperm.h"
#include "ifat_common.h"
#include "bdri.h"
#include "image.h"
#include "language.h"
#include "nandcmac.h"
#include "ui.h"
#include "vff.h"

#define FAT_ENTRY_SIZE sizeof(IFatEntry)
#define REPLACE_SIZE_MISMATCH 2

#define getfatflag(uv) (((uv) & 0x80000000UL) != 0)
#define getfatindex(uv) ((uv) & 0x7FFFFFFFUL)
#define buildfatuv(index, flag) ((index) | ((flag) ? 0x80000000UL : 0))

typedef struct {
    IFatPreHeader pre_header;
    IFatFsInfo fs_info;
} PACKED_STRUCT BDRIFsHeader;

typedef struct {
    char magic[8]; // varies based on media type and importdb vs titledb
    u32 version; // should always be 0
    u8 reserved[0x74];
} PACKED_STRUCT TitleDBPreHeader;

typedef struct {
    TitleDBPreHeader pre_header;
    BDRIFsHeader fs_header;
} PACKED_STRUCT TitleDBHeader;

typedef struct {
    char magic[4]; // "TICK"
    u32 version; // always 1
    u8 padding[8];
} PACKED_STRUCT TickDBPreHeader;

typedef struct {
    TickDBPreHeader pre_header;
    BDRIFsHeader fs_header;
} PACKED_STRUCT TickDBHeader;

typedef struct {
    u32 parent_index;
    u8 title_id[8];
    u32 next_sibling_index;
    u8 padding1[4];
    u32 start_block_index;
    u64 size; // in bytes
    u8 padding2[8];
    u32 hash_bucket_next_index;
} __attribute__((packed)) BDRIFileEntry;

typedef struct {
    u32 total_entry_count;
    u32 max_entry_count; // == max_file_count + 1
    u8 padding[32];
    u32 next_dummy_index;
} __attribute__((packed)) BDRIDummyFileEntry;

typedef struct {
    u64 id;
    u32 next_sibling_index;
    u32 first_subdir_index;
    u32 first_subfile_index;
    u8 pad0[4];
    u32 hash_bucket_next_index;
} __attribute__((packed)) BDRIDirectoryEntry;

typedef struct {
    u32 total_entry_count;
    u32 max_entry_count;
    u8 padding[16];
    u32 next_dummy_index;
} __attribute__((packed)) BDRIDummyDirectoryEntry;

typedef struct {
    u32 ticket_count; // usually == 1
    u32 ticket_size; // usually == 0x350 == sizeof(Ticket)
    Ticket ticket;
} __attribute__((packed, aligned(4))) TicketEntry;

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
    const TitleDBHeader* title = (TitleDBHeader*) (void *) pre_header;
    const TickDBHeader* tick = (TickDBHeader*) (void *) pre_header;

    return (tickdb ? ((strncmp(tick->pre_header.magic, "TICK", 4) == 0) && (tick->pre_header.version == 1)) :
        ((strcmp(title->pre_header.magic, "NANDIDB") == 0) || (strcmp(title->pre_header.magic, "NANDTDB") == 0) ||
         (strcmp(title->pre_header.magic, "TEMPIDB") == 0) || (strcmp(title->pre_header.magic, "TEMPTDB") == 0))) &&
         (strcmp((tickdb ? tick->fs_header : title->fs_header).pre_header.magic_str, "BDRI") == 0) &&
         ((tickdb ? tick->fs_header : title->fs_header).pre_header.version == 0x30000);
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
    if ((fs_header->pre_header.fs_info_offset != 0x20) ||
        (fs_header->fs_info.fat.outfat_count != fs_header->fs_info.data_region.outfat_count)) // Could be more thorough
        return 1;

    const u32 data_offset = fs_header_offset + fs_header->fs_info.data_region.outfat_offset;
    const u32 fet_offset = data_offset + fs_header->fs_info.filetable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fht_offset = fs_header_offset + fs_header->fs_info.file_hashtbl.outfat_offset;

    u32 index = 0;
    BDRIFileEntry file_entry;
    u64 tid_be = getbe64(title_id);
    u8* title_id_be = (u8*) &tid_be;
    const u32 hash_bucket = GetHashBucket(title_id_be, 1, fs_header->fs_info.file_hashtbl.outfat_count);

    if (BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &(file_entry.hash_bucket_next_index)) != FR_OK)
        return 1;

    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        if (file_entry.hash_bucket_next_index == 0)
            return 1;

        index = file_entry.hash_bucket_next_index;

        if (BDRIRead(fet_offset + index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);

    *size = file_entry.size;

    return 0;
}

static u32 ReadBDRIEntry(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, u8* entry, const u32 expected_size) {
    if ((fs_header->pre_header.fs_info_offset != 0x20) ||
        (fs_header->fs_info.fat.outfat_count != fs_header->fs_info.data_region.outfat_count)) // Could be more thorough
        return 1;

    const u32 data_offset = fs_header_offset + fs_header->fs_info.data_region.outfat_offset;
    const u32 fet_offset = data_offset + fs_header->fs_info.filetable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fht_offset = fs_header_offset + fs_header->fs_info.file_hashtbl.outfat_offset;
    const u32 fat_offset = fs_header_offset + fs_header->fs_info.fat.outfat_offset;

    u32 index = 0;
    BDRIFileEntry file_entry;
    u64 tid_be = getbe64(title_id);
    u8* title_id_be = (u8*) &tid_be;
    const u32 hash_bucket = GetHashBucket(title_id_be, 1, fs_header->fs_info.file_hashtbl.outfat_count);

    if (BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &(file_entry.hash_bucket_next_index)) != FR_OK)
        return 1;

    // Find the file entry for the tid specified, fail if it doesn't exist
    do {
        if (file_entry.hash_bucket_next_index == 0)
            return 1;

        index = file_entry.hash_bucket_next_index;

        if (BDRIRead(fet_offset + index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);

    if (expected_size && (file_entry.size != expected_size))
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

        u32 btr = min(file_entry.size - bytes_read, read_count * fs_header->fs_info.data_region_blocksize);
        if (entry && (BDRIRead(data_offset + read_start * fs_header->fs_info.data_region_blocksize, btr, entry + bytes_read) != FR_OK))
            return 1;

        bytes_read += btr;
    }

    return 0;
}

static u32 RemoveBDRIEntry(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id) {
    if ((fs_header->pre_header.fs_info_offset != 0x20) ||
        (fs_header->fs_info.fat.outfat_count != fs_header->fs_info.data_region.outfat_count)) // Could be more thorough
        return 1;

    const u32 data_offset = fs_header_offset + fs_header->fs_info.data_region.outfat_offset;
    const u32 det_offset = data_offset + fs_header->fs_info.dirtable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fet_offset = data_offset + fs_header->fs_info.filetable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fht_offset = fs_header_offset + fs_header->fs_info.file_hashtbl.outfat_offset;
    const u32 fat_offset = fs_header_offset + fs_header->fs_info.fat.outfat_offset;

    u32 index = 0, previous_index = 0;
    BDRIFileEntry file_entry;
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

        if (BDRIRead(fet_offset + index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 1;
    } while (memcmp(title_id_be, file_entry.title_id, 8) != 0);

    BDRIDummyFileEntry dummy_entry;

    // Read the 0th entry in the FET, which is always a dummy entry
    if (BDRIRead(fet_offset, sizeof(BDRIDummyFileEntry), &dummy_entry) != FR_OK)
        return 1;

    if (dummy_entry.max_entry_count != fs_header->fs_info.filetable_info.max_entry_count + 1)
        return 1;

    if ((BDRIWrite(fet_offset + index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &dummy_entry) != FR_OK) ||
        (BDRIWrite(fet_offset + 0x28, sizeof(u32), &index) != FR_OK) ||
        (BDRIWrite((previous_index == 0) ? det_offset + 0x2C : fet_offset + previous_index * sizeof(BDRIFileEntry) + 0xC, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK))
        return 1;

    const u32 hash_bucket = GetHashBucket(file_entry.title_id, file_entry.parent_index, fs_header->fs_info.file_hashtbl.outfat_count);
    u32 index_hash = 0;

    if (BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != FR_OK)
        return 1;

    if (index_hash == index) {
        if (BDRIWrite(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &(file_entry.hash_bucket_next_index)) != FR_OK)
            return 1;
    } else {
        u32 prev_hash_index = 0;
        do {
            if (index_hash == 0) // This shouldn't happen if the entry was properly added
                break;

            prev_hash_index = index_hash;
            if (BDRIRead(fet_offset + index_hash * sizeof(BDRIFileEntry) + 0x28, sizeof(u32), &index_hash) != FR_OK)
                return 1;
        } while (index_hash != index);

        if ((prev_hash_index != 0) && BDRIWrite(fet_offset + prev_hash_index * sizeof(BDRIFileEntry) + 0x28, sizeof(u32), &(file_entry.hash_bucket_next_index)) != FR_OK)
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

    // Bug fix: use buildfatuv to explicitly clear Bit 31 (the multi-block flag).
    // If the tail of the freed chain is a multi-block node start, Bit 31 is already
    // set in fat_entry[1]. A plain |= would keep it set, corrupting the free list.
    fat_entry[1] = buildfatuv(next_free_index, false);

    if (BDRIWrite(fat_offset + fat_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
        return 1;

    // Bug fix: guard against next_free_index == 0 (freed entry was the last free
    // block). Without this guard, FAT[0] (the free-list sentinel) gets corrupted
    // by a stray back-pointer write.
    if (next_free_index != 0) {
        if (BDRIRead(fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;

        fat_entry[0] = buildfatuv(fat_index, false);

        if (BDRIWrite(fat_offset + next_free_index * FAT_ENTRY_SIZE, FAT_ENTRY_SIZE, fat_entry) != FR_OK)
            return 1;
    }

    return 0;
}

static u32 AllocateBDRIBlocks(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u32 size_blocks, u32 *out_fat_index) {
    u32 fat_entry[2];
    u32 fat_index = 0;
    const u32 fat_offset = fs_header_offset + fs_header->fs_info.fat.outfat_offset;

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

    *out_fat_index = fat_index;
    return 0;
}

static u32 AddBDRIEntry(const BDRIFsHeader* fs_header, const u32 fs_header_offset, const u8* title_id, const u8* entry, const u32 size, bool replace) {
    if ((fs_header->pre_header.fs_info_offset != 0x20) ||
        (fs_header->fs_info.fat.outfat_count != fs_header->fs_info.data_region.outfat_count)) // Could be more thorough
        return 1;

    if (!entry || !size)
        return 1;

    const u32 data_offset = fs_header_offset + fs_header->fs_info.data_region.outfat_offset;
    const u32 det_offset = data_offset + fs_header->fs_info.dirtable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fet_offset = data_offset + fs_header->fs_info.filetable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fht_offset = fs_header_offset + fs_header->fs_info.file_hashtbl.outfat_offset;
    const u32 fat_offset = fs_header_offset + fs_header->fs_info.fat.outfat_offset;
    const u32 size_blocks = (size / fs_header->fs_info.data_region_blocksize) + (((size % fs_header->fs_info.data_region_blocksize) == 0) ? 0 : 1);

    u32 index = 0, max_index = 0;
    BDRIFileEntry file_entry;
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

        if (BDRIRead(fet_offset + index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 1;

        // If an entry for the tid already existed that is already the specified size and replace was specified, just replace the existing entry
        if (memcmp(title_id_be, file_entry.title_id, 8) == 0) {
            if (!replace) return 1;
            else if (file_entry.size != size) return REPLACE_SIZE_MISMATCH;
            else {
                do_replace = true;
                break;
            }
        }
    }

    u32 fat_entry[2];
    u32 fat_index = 0;

    if (!do_replace) {
        if (AllocateBDRIBlocks(fs_header, fs_header_offset, size_blocks, &fat_index) != 0)
            return 1;
    } else {
        fat_index = file_entry.start_block_index + 1;
    }

    u32 bytes_written = 0, fat_index_write = fat_index;

    while (bytes_written < size) { // Write the full entry, walking the FAT node chain
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

        u32 btw = min(size - bytes_written, write_count * fs_header->fs_info.data_region_blocksize);
        if (BDRIWrite(data_offset + write_start * fs_header->fs_info.data_region_blocksize, btw, entry + bytes_written) != FR_OK)
            return 1;

        bytes_written += btw;
    }

    if (!do_replace) {
        BDRIDummyFileEntry dummy_entry;

        // Read the 0th entry in the FET, which is always a dummy entry
        if (BDRIRead(fet_offset, sizeof(BDRIDummyFileEntry), &dummy_entry) != FR_OK)
            return 1;

        if (dummy_entry.max_entry_count != fs_header->fs_info.filetable_info.max_entry_count + 1)
            return 1;

        if (dummy_entry.next_dummy_index == 0) { // If the 0th entry is the only dummy entry, make a new entry
            file_entry.next_sibling_index = max_index + 1;

            dummy_entry.total_entry_count++;
            if (BDRIWrite(fet_offset, sizeof(u32), &(dummy_entry.total_entry_count)) != FR_OK)
                return 1;
        } else { // If there's at least one extraneous dummy entry, replace it
            file_entry.next_sibling_index = dummy_entry.next_dummy_index;

            if ((BDRIRead(fet_offset + dummy_entry.next_dummy_index * sizeof(BDRIDummyFileEntry), sizeof(BDRIDummyFileEntry), &dummy_entry) != FR_OK) ||
                (BDRIWrite(fet_offset, sizeof(BDRIDummyFileEntry), &dummy_entry) != FR_OK))
                return 1;
        }

        if (BDRIWrite((index == 0) ? det_offset + 0x2C : fet_offset + index * sizeof(BDRIFileEntry) + 0xC, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
            return 1;

        index = file_entry.next_sibling_index;

        const u32 hash_bucket = GetHashBucket(title_id_be, 1, fs_header->fs_info.file_hashtbl.outfat_count);
        u32 index_hash = 0;

        if ((BDRIRead(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index_hash) != FR_OK) ||
            (BDRIWrite(fht_offset + hash_bucket * sizeof(u32), sizeof(u32), &index) != FR_OK))
            return 1;

        memset(&file_entry, 0, sizeof(BDRIFileEntry));
        file_entry.parent_index = 1;
        memcpy(file_entry.title_id, title_id_be, 8);
        file_entry.start_block_index = fat_index - 1;
        file_entry.size = (u64) size;
        file_entry.hash_bucket_next_index = index_hash;

        if (BDRIWrite(fet_offset + index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 1;
    }

    return 0;
}

static u32 GetNumBDRIEntries(const BDRIFsHeader* fs_header, const u32 fs_header_offset) {
    if ((fs_header->pre_header.fs_info_offset != 0x20) ||
        (fs_header->fs_info.fat.outfat_count != fs_header->fs_info.data_region.outfat_count)) // Could be more thorough
        return 0;

    const u32 data_offset = fs_header_offset + fs_header->fs_info.data_region.outfat_offset;
    const u32 det_offset = data_offset + fs_header->fs_info.dirtable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fet_offset = data_offset + fs_header->fs_info.filetable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;

    u32 num_entries = 0;
    BDRIFileEntry file_entry;

    // Read the index of the first file entry from the directory entry table
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 0;

    while (file_entry.next_sibling_index != 0) {
        num_entries++;
        if (BDRIRead(fet_offset + file_entry.next_sibling_index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 0;
    }

    return num_entries;
}

static u32 ListBDRIEntryTitleIDs(const BDRIFsHeader* fs_header, const u32 fs_header_offset, u8* title_ids, u32 max_title_ids) {
    if ((fs_header->pre_header.fs_info_offset != 0x20) ||
        (fs_header->fs_info.fat.outfat_count != fs_header->fs_info.data_region.outfat_count))
        return 0;

    const u32 data_offset = fs_header_offset + fs_header->fs_info.data_region.outfat_offset;
    const u32 det_offset = data_offset + fs_header->fs_info.dirtable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;
    const u32 fet_offset = data_offset + fs_header->fs_info.filetable_info.starting_block_index * fs_header->fs_info.data_region_blocksize;

    u32 num_entries = 0;
    BDRIFileEntry file_entry;

    for (u32 i = 0; i < max_title_ids; i++)
        memset(title_ids, 0, max_title_ids * 8);

    // Read the index of the first file entry from the directory entry table
    if (BDRIRead(det_offset + 0x2C, sizeof(u32), &(file_entry.next_sibling_index)) != FR_OK)
        return 1;

    while ((file_entry.next_sibling_index != 0) && (num_entries < max_title_ids)) {
        if (BDRIRead(fet_offset + file_entry.next_sibling_index * sizeof(BDRIFileEntry), sizeof(BDRIFileEntry), &file_entry) != FR_OK)
            return 1;

        u64 tid_be = getbe64(file_entry.title_id);
        memcpy(title_ids + num_entries * 8, (u8*) &tid_be, 8);

        num_entries++;
    }

    return 0;
}

u32 GetNumTitleInfoEntries(const char* path) {
    FIL file;
    TitleDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 0;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TitleDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 0;
    }

    u32 num = GetNumBDRIEntries(&(pre_header.fs_header), sizeof(TitleDBHeader) - sizeof(BDRIFsHeader));

    fvx_close(bdrifp);
    bdrifp = NULL;
    return num;
}

u32 GetNumTickets(const char* path) {
    FIL file;
    TickDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 0;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TickDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 0;
    }

    u32 num = GetNumBDRIEntries(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader));

    fvx_close(bdrifp);
    bdrifp = NULL;
    return num;
}

u32 ListTitleInfoEntryTitleIDs(const char* path, u8* title_ids, u32 max_title_ids) {
    FIL file;
    TitleDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TitleDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (ListBDRIEntryTitleIDs(&(pre_header.fs_header), sizeof(TitleDBHeader) - sizeof(BDRIFsHeader), title_ids, max_title_ids) != 0)) {
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
    TickDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TickDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (ListBDRIEntryTitleIDs(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_ids, max_title_ids) != 0)) {
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
    TitleDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TitleDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (ReadBDRIEntry(&(pre_header.fs_header), sizeof(TitleDBHeader) - sizeof(BDRIFsHeader), title_id, (u8*) tie,
            sizeof(TitleInfoEntry)) != 0)) {
        fvx_close(bdrifp);
        bdrifp = NULL;
        return 1;
    }

    fvx_close(bdrifp);
    bdrifp = NULL;
    return 0;
}

u32 ReadTicketFromDB(const char* path, const u8* title_id, Ticket** ticket) {
    FIL file;
    TickDBHeader pre_header;
    TicketEntry* te = NULL;
    u32 entry_size;

    *ticket = NULL;
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TickDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (GetBDRIEntrySize(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_id, &entry_size) != 0) ||
        entry_size < sizeof(TicketEntry) + 0x14 ||
        (te = (TicketEntry*)malloc(entry_size), te == NULL) ||
        (ReadBDRIEntry(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_id, (u8*) te,
            entry_size) != 0)) {
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
    TitleDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TitleDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (RemoveBDRIEntry(&(pre_header.fs_header), sizeof(TitleDBHeader) - sizeof(BDRIFsHeader), title_id) != 0)) {
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
    TickDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TickDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        (RemoveBDRIEntry(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_id) != 0)) {
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
    TitleDBHeader pre_header;

    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    bdrifp = &file;

    if ((BDRIRead(0, sizeof(TitleDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, false) ||
        (AddBDRIEntry(&(pre_header.fs_header), sizeof(TitleDBHeader) - sizeof(BDRIFsHeader), title_id,
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
    TickDBHeader pre_header;
    u32 entry_size = sizeof(TicketEntry) + GetTicketContentIndexSize(ticket);

    TicketEntry* te = (TicketEntry*)malloc(entry_size);
    if (!te) {
        return 1;
    }

    te->ticket_count = 1;
    te->ticket_size = GetTicketSize(ticket);
    memcpy(&te->ticket, ticket, te->ticket_size);
    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) {
        free(te);
        return 1;
    }

    bdrifp = &file;

    u32 add_bdri_res = 0;

    if ((BDRIRead(0, sizeof(TickDBHeader), &pre_header) != FR_OK) ||
        !CheckDBMagic((u8*) &pre_header, true) ||
        ((add_bdri_res = AddBDRIEntry(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_id,
            (const u8*) te, entry_size, replace)) == 1) ||
        (add_bdri_res == REPLACE_SIZE_MISMATCH && ((RemoveBDRIEntry(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_id) != 0) ||
            (AddBDRIEntry(&(pre_header.fs_header), sizeof(TickDBHeader) - sizeof(BDRIFsHeader), title_id,
            (const u8*) te, entry_size, replace) != 0)))) {
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

static u32 CalcNumHashBuckets(u32 num_entries)
{
    if (num_entries <= 3)
        return 3;

    if (num_entries <= 19)
        return num_entries | 1;

    for (int i = 0; i < 100; ++i)
    {
        u32 ret = num_entries + i;

        if (ret & 1 && ret % 3 && ret % 5 && ret % 7 && ret % 11 && ret % 13 && ret % 17)
            return ret;
    }

    return num_entries | 1;
}

u32 CreateBDRI(const char *path, u64 image_offset, u64 image_size, u32 blocksize, u32 num_files) {
    BDRIFsHeader bdri;
    memset(&bdri, 0, sizeof(BDRIFsHeader));

    IFatPreHeader *pre_header = &bdri.pre_header;
    IFatFsInfo *fs_info = &bdri.fs_info;

    const u64 img_num_blocks = image_size / blocksize;
    const u32 file_hashbucket_count = CalcNumHashBuckets(num_files);

    memcpy(pre_header->magic_str, "BDRI", 4);
    pre_header->version = 0x30000;
    pre_header->fs_image_blocksize = blocksize;
    pre_header->fs_image_size_blocks = img_num_blocks;
    pre_header->fs_info_offset = sizeof(IFatPreHeader);

    const u64 full_image_size = pre_header->fs_image_blocksize * pre_header->fs_image_size_blocks;

    u32 num_data_blocks = img_num_blocks;
    u32 data_region_offs = 0;

    // dynamically adjust the number of data region blocks to fit inside the given image size
    while (1) {
        if (!num_data_blocks)
            return 1; // image will not fit inside given size

        data_region_offs =
            align_pow2(
                sizeof(IFatPreHeader) + sizeof(IFatFsInfo) + /* headers */
                (4 * 1) + (4 * file_hashbucket_count) +      /* hashtables (dir, file) */
                (num_data_blocks + 1) * sizeof(IFatEntry),   /* FAT entries, including the freelist entry */
            blocksize);

        if (full_image_size >= (num_data_blocks * blocksize) + data_region_offs)
            break; // found a satisfactory number of data blocks

        --num_data_blocks;
    }

    // create file system info

    fs_info->data_region_blocksize = blocksize;

    fs_info->dir_hashtbl.outfat_offset = pre_header->fs_info_offset + sizeof(IFatFsInfo);
    fs_info->dir_hashtbl.outfat_count = 1;

    fs_info->file_hashtbl.outfat_offset = fs_info->dir_hashtbl.outfat_offset + 4;
    fs_info->file_hashtbl.outfat_count = file_hashbucket_count;

    fs_info->fat.outfat_offset = fs_info->file_hashtbl.outfat_offset + file_hashbucket_count * 4;
    fs_info->fat.outfat_count = num_data_blocks;

    fs_info->data_region.outfat_offset = data_region_offs;
    fs_info->data_region.outfat_count = num_data_blocks;

    // dummy freelist dir entry [0] + root dir entry [1]
    uint32_t dir_entries_num_blocks  = ceil_div(2 * sizeof(BDRIDirectoryEntry), blocksize);

    // dummy freelist file entry [0] + file entries [n]
    uint32_t file_entries_num_blocks = ceil_div((num_files + 1) * sizeof(BDRIFileEntry), blocksize);

    fs_info->dirtable_info.max_entry_count = 1;
    fs_info->dirtable_info.block_count = dir_entries_num_blocks;
    fs_info->dirtable_info.starting_block_index = 0xFFFFFFFF; // to be allocated in FAT later using above block_count

    fs_info->filetable_info.max_entry_count = num_files;
    fs_info->filetable_info.block_count = file_entries_num_blocks;
    fs_info->filetable_info.starting_block_index = 0xFFFFFFFF; // to be allocated in FAT later using above block_count

    FIL file;

    if (fvx_open(&file, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) {
        bdrifp = NULL;
        return 1;
    }

    bdrifp = &file;

    // create new FAT

    u32 fatentries[2][2];

    fatentries[0][0] = buildfatuv(0, 0);
    fatentries[0][1] = buildfatuv(1, 0);

    fatentries[1][0] = buildfatuv(0, true);
    fatentries[1][1] = buildfatuv(0, num_data_blocks > 1);

#define FATOFFSET(n) (image_offset + fs_info->fat.outfat_offset + (n) * FAT_ENTRY_SIZE)

    if (BDRIWrite(FATOFFSET(0), FAT_ENTRY_SIZE * 2, fatentries) != FR_OK) {
        fvx_close(&file);
        bdrifp = NULL;
        return 1;
    }

    if (num_data_blocks > 1) {
        u32 *fatentry = fatentries[0];

        fatentry[0] = buildfatuv(1, true);
        fatentry[1] = buildfatuv(num_data_blocks, false);

        if (BDRIWrite(FATOFFSET(2), FAT_ENTRY_SIZE, fatentry) != FR_OK ||
            BDRIWrite(FATOFFSET(num_data_blocks), FAT_ENTRY_SIZE, fatentry) != FR_OK) {
            fvx_close(&file);
            bdrifp = NULL;
            return 1;
        }
    }

    // allocate file and directory entry tables in FAT

    if (AllocateBDRIBlocks(&bdri, image_offset, dir_entries_num_blocks, &fs_info->dirtable_info.starting_block_index) != 0 ||
        AllocateBDRIBlocks(&bdri, image_offset, file_entries_num_blocks, &fs_info->filetable_info.starting_block_index) != 0) {
        fvx_close(&file);
        bdrifp = NULL;
        return 1;
    }

    // because of the extra freelist sentinel in fat[0], the actual data region index is fat_index - 1
    fs_info->dirtable_info.starting_block_index -= 1;
    fs_info->filetable_info.starting_block_index -= 1;

    if (BDRIWrite(image_offset, sizeof(BDRIFsHeader), &bdri) != FR_OK) {
        fvx_close(&file);
        bdrifp = NULL;
        return 1;
    }

    // initialize file/dir hash tables & entries

    // the directory hashtable has just one entry, pointing to the root directory
    // no need to do the calculation
    u32 dir_hash_value = 1;

    if (BDRIWrite(image_offset + fs_info->dir_hashtbl.outfat_offset, sizeof(u32), &dir_hash_value) != FR_OK) {
        fvx_close(&file);
        bdrifp = NULL;
        return 1;
    }

    u8 zero[0x200];
    memset(zero, 0, sizeof(zero));

    u32 remaining_fht = file_hashbucket_count * 4;
    u32 fht_cur_offset = fs_info->file_hashtbl.outfat_offset;

    // fill the file hashtable with zeros
    while (remaining_fht) {
        u32 writesize = min(remaining_fht, sizeof(zero));

        if (BDRIWrite(image_offset + fht_cur_offset, writesize, zero) != FR_OK) {
            fvx_close(&file);
            bdrifp = NULL;
            return 1;
        }

        remaining_fht -= writesize;
        fht_cur_offset += writesize;
    }

    // initialize directory entry table
    BDRIDummyDirectoryEntry dummy_dir;
    BDRIDirectoryEntry root_dir;
    memset(&dummy_dir, 0, sizeof(dummy_dir));
    memset(&root_dir, 0, sizeof(root_dir));

    dummy_dir.next_dummy_index = 0;
    dummy_dir.total_entry_count = 2; // this (dummy), and root (the entry after)
    dummy_dir.max_entry_count = 3; // even though only two are "used"

    u64 det_offset = fs_info->data_region.outfat_offset + fs_info->dirtable_info.starting_block_index * fs_info->data_region_blocksize;

    if (BDRIWrite(image_offset + det_offset, sizeof(dummy_dir), &dummy_dir) != FR_OK ||
        BDRIWrite(image_offset + det_offset + sizeof(dummy_dir), sizeof(root_dir), &root_dir) != FR_OK) {
        fvx_close(&file);
        bdrifp = NULL;
        return 1;
    }

    // initialize file entry table
    BDRIDummyFileEntry dummy_file;
    memset(&dummy_file, 0, sizeof(dummy_file));

    dummy_file.next_dummy_index = 0;
    dummy_file.total_entry_count = 1; // currently, just this (dummy)
    dummy_file.max_entry_count = num_files + 1; // this (dummy), and however many file entries should exist

    u64 fet_offset = fs_info->data_region.outfat_offset + fs_info->filetable_info.starting_block_index * fs_info->data_region_blocksize;

    if (BDRIWrite(image_offset + fet_offset, sizeof(dummy_file), &dummy_file) != FR_OK) {
        fvx_close(&file);
        bdrifp = NULL;
        return 1;
    }

    fvx_close(&file);
    bdrifp = NULL;

    return 0;
}

typedef struct DbFileConfig {
    const char *path;
    const char *magic;
    u32 version;
    u32 full_data_size;
    u32 image_offset; // also pre-header size
    u32 image_size;
    u32 image_blocksize;
    u32 num_entries;
} DbFileConfig;

typedef enum DbType {
    NAND_TICKET_DB = 0, NAND_IMPORT_DB, NAND_TITLE_DB, NAND_TMP_I_DB, NAND_TMP_T_DB,
    SDMC_TITLE_DB, SDMC_IMPORT_DB,
} DbType;

static const DbFileConfig DbConfigs[7] = {
    // NAND dbs
    { "/dbs/ticket.db", "TICK"   , 1, 0x10A2010, 0x10, 0x10A2000, 0x200, 8192 }, // ticket.db is the only db where the image size is correct...
    { "/dbs/import.db", "NANDIDB", 0, 0x17800  , 0x80, 0x17800  , 0x80 , 512  }, // ---
    { "/dbs/title.db" , "NANDTDB", 0, 0x2ED80  , 0x80, 0x2ED80  , 0x80,  1024 }, //   | *all* of these are missing 0x80 bytes at the end.
    { "/dbs/tmp_i.db" , "TEMPIDB", 0, 0xE00    , 0x80, 0xE00    , 0x80,  16   }, //   | the image size should be file size - 0x80 because
    { "/dbs/tmp_t.db" , "TEMPIDB", 0, 0x6000   , 0x80, 0x6000   , 0x80,  128  }, //   | of the pre-header. keeping it the same just for
    // SDMC dbs                                                                  //   | consistency with Process9, really.
    { "/dbs/title.db" , "TEMPTDB", 0, 0x175A80 , 0x80, 0x175A80 , 0x80,  8192 }, //   |
    { "/dbs/import.db", "TEMPTDB", 0, 0x175A80 , 0x80, 0x175A80 , 0x80 , 8192 }, // ---
};

static const DbFileConfig *GetDbFileConfigBuildPath(char *outpath, u32 pathsize, u32 type, bool emunand) {
    if (type >= countof(DbConfigs))
        return NULL;

    bool nand = type <= NAND_TMP_T_DB;

    char dest_drive =
        nand ?
            emunand ? '4' : '1' :
            emunand ? 'B' : 'A';

    const DbFileConfig *config = &DbConfigs[type];

    snprintf(outpath, pathsize, "%c:%s", dest_drive, config->path);

    return config;
}

u32 CreateDbFile(u32 type, bool emunand) {
    char path[256] = { 0 };

    const DbFileConfig *config = GetDbFileConfigBuildPath(path, sizeof(path), type, emunand);
    if (!config)
        return 1;

    u64 reqsize = BuildDiffCalcRequiredSize(config->full_data_size, false, true);

    if (!CheckWritePermissions(path))
        return 1;

    // ensure remounting the old mount path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;

    // unmount any mounted image since we're about to create a DIFF that will need to be mounted
    InitImgFS(NULL);

    // create the raw file
    if (fvx_qcreate(path, reqsize) != FR_OK) {
        InitImgFS(path_bak);
        return 1;
    }

    // create the DIFF container and mount it
    if (CreateDiff(path, config->full_data_size, false, true, NULL) != 0 ||
        !InitImgFS(path)) {
        InitImgFS(path_bak);
        fvx_unlink(path);
        return 1;
    }

    char db_hdr[0x80];
    memset(db_hdr, 0, sizeof(db_hdr));
    u32 version_offset = align_pow2(strlen(config->magic), 4);
    memcpy(db_hdr, config->magic, strlen(config->magic));
    memcpy(&db_hdr[version_offset], &config->version, 4);

    UINT written = 0;

    // write the BDRI filesystem image
    if (fvx_qwrite("D:/partitionA.bin", db_hdr, 0, config->image_offset, &written) != 0 ||
        CreateBDRI("D:/partitionA.bin", config->image_offset, config->image_size, config->image_blocksize, config->num_entries) != 0) {
        InitImgFS(path_bak);
        fvx_unlink(path);
        return 1;
    }

    // remount previously mounted image, if there was one
    InitImgFS(path_bak);

    // make sure CMAC is fixed
    if (FixFileCmac(path, true) != 0) {
        fvx_unlink(path);
        return 1;
    }

    return 0;
}

u32 CreateDbFilesForDrive(const char *destdrv, bool silent, bool force_overwrite) {
    static const DbType db_types_nand[5] = { NAND_TICKET_DB, NAND_IMPORT_DB, NAND_TITLE_DB, NAND_TMP_I_DB, NAND_TMP_T_DB };
    static const DbType db_types_sdmc[2] = { SDMC_TITLE_DB, SDMC_IMPORT_DB };

    if (*destdrv != 'A' && *destdrv != 'B' && *destdrv != '1' && *destdrv != '4') {
        return 1;
    }

    bool emunand = *destdrv == 'B' /* EmuNAND SD */ || *destdrv == '4' /* EmuNAND CTRNAND */;
    bool sd = *destdrv == 'A' || *destdrv == 'B' /* Sys/EmuNAND SD */;
    const DbType *dbs_to_check = sd ? db_types_sdmc : db_types_nand;
    u32 db_count = sd ? countof(db_types_sdmc) : countof(db_types_nand);

    u32 num_to_create = db_count;
    u32 num_already_exist = 0;
    u32 num_failed = 0;
    u32 num_created_ok = 0;

    char db_path[256] = { 0 };
    const DbFileConfig *config = NULL;

    for (u32 i = 0; i < db_count; i++) {
        if (!(config = GetDbFileConfigBuildPath(db_path, sizeof(db_path), dbs_to_check[i], emunand))) {
            return 1;
        }

        if (fvx_stat(db_path, NULL) == FR_OK) {
            if (!force_overwrite && fvx_qsize(db_path) == BuildDiffCalcRequiredSize(config->full_data_size, false, true)) {
                ++num_already_exist;
                --num_to_create;
                continue;
            }
        }

        if (CreateDbFile(dbs_to_check[i], emunand) == 0) {
            ++num_created_ok;
        } else {
            ++num_failed;
            if (!silent) {
                ShowPrompt(false, STR_PATH_CREATE_DB_FILE_FAILED, db_path);
            }
        }
    }

    if (!silent) {
        if (num_already_exist) {
            if (num_already_exist == db_count) {
                ShowPrompt(false, "%s", STR_NO_MISSING_DB_FILES_DETECTED);
            } else {
                ShowPrompt(false, STR_CREATE_DB_FILES_N_N_N_OK_FAILED_ALREADY_EXIST, num_created_ok, num_failed, num_already_exist);
            }
        } else {
            if (num_created_ok == db_count) {
                ShowPrompt(false, "%s", STR_SUCCESSFULLY_CREATED_DB_FILES_FOR_DRIVE);
            } else {
                ShowPrompt(false, STR_CREATE_DB_FILES_N_N_OK_FAILED, num_created_ok, num_failed);
            }
        }
    }

    return num_to_create == num_created_ok ? 0 : 1;
}