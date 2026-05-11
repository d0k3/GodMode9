#include "saveexsv.h"
#include "common.h"
#include "fsutil.h"
#include "types.h"
#include "vff.h"
#include "ff.h"

#define PART_A_PATH "D:/partitionA.bin"
#define PART_B_PATH "D:/partitionB.bin"

static int read_alloc_chunk(void **output, FIL *file, u64 offset, u32 size) {
    UINT nread = 0;
    if (!(*output = malloc(size)) ||
        fvx_lseek(file, offset) != FR_OK ||
        fvx_read(file, *output, size, &nread) != FR_OK || nread != size) {
        return 1;
    }
    return 0;
}

static FIL part_a = { 0 }, part_b = { 0 };
static FIL *data_part = NULL;

int SaveExsvFileInit(SaveExsvFile *sav) {
    UINT nread = 0;
    int retval = 0;

    // partitionA is mandatory
    if (fvx_open(&part_a, PART_A_PATH, FA_READ) != FR_OK)
        goto err_exit;
    
    
    if (PathExist(PART_B_PATH)) {
        // if partitionB exists, we need it
        if (fvx_open(&part_b, PART_B_PATH, FA_READ) != FR_OK)
            goto err_exit;

        data_part = &part_b;
    } else {
        data_part = &part_a;
    }

    sav->duplicate_meta = data_part == &part_b;

    u32 dir_table_offset = 0, dir_table_size = 0;
    u32 file_table_offset = 0, file_table_size = 0;

    if (fvx_read(&part_a, &sav->pre_header, sizeof(SavePreHeader), &nread) != FR_OK || nread != sizeof(SavePreHeader))
        goto err_exit;

    u32 full_header_size = sizeof(SavePreHeader) + sizeof(SaveFsInfo);

    if (sav->pre_header.magic == 0x45564153 ) { /* SAVE */
        if (sav->pre_header.version != 0x40000 || sav->pre_header.fs_info_offset != 0x20)
            goto err_exit;
    } else if (sav->pre_header.magic == 0x45585356) { /* EXSV (VSXE) */
        if (sav->pre_header.version != 0x30000 || sav->pre_header.fs_info_offset != 0x138 ||
            fvx_read(&part_a, &sav->exsv_extra_hdr, sizeof(ExsvExtraHeader), &nread) != FR_OK || nread != sizeof(ExsvExtraHeader))
            goto err_exit;

        full_header_size += sizeof(ExsvExtraHeader);
        sav->is_exsv = true;
    } else {
        goto err_exit;
    }

    if (fvx_read(&part_a, &sav->fs_info, sizeof(SaveFsInfo), &nread) != FR_OK || nread != sizeof(SaveFsInfo))
        goto err_exit;

    u64 part_a_size = fvx_size(&part_a);

    if (!sav->duplicate_meta) {

        dir_table_offset = sav->fs_info.data_region.offset + sav->fs_info.dirtable_info.dupdata.starting_block_index * sav->fs_info.data_region_blocksize;
        dir_table_size = sav->fs_info.data_region_blocksize * sav->fs_info.dirtable_info.dupdata.block_count;
        
        file_table_offset = sav->fs_info.data_region.offset + sav->fs_info.filetable_info.dupdata.starting_block_index * sav->fs_info.data_region_blocksize;
        file_table_size = sav->fs_info.data_region_blocksize * sav->fs_info.filetable_info.dupdata.block_count;

        // duplicate data has the dir and file tables in the data region, so we don't need to count them separately
        u64 min_part_a_size = align(full_header_size +
                                    sav->fs_info.dir_hashtbl.count * sizeof(u32) +
                                    sav->fs_info.file_hashtbl.count * sizeof(u32) +
                                    (sav->fs_info.fat.count + 1) * sizeof(SaveFatEntry),
                                    sav->pre_header.fs_image_blocksize) +
                              sav->fs_info.data_region.count * sav->fs_info.data_region_blocksize;

        if (part_a_size < min_part_a_size) {
            goto err_exit;
        }
    } else {
        // it should not be possible for the extdata main DIFF to be duplicate meta
        if (sav->is_exsv)
            goto err_exit;

        dir_table_offset = sav->fs_info.dirtable_info.dupmeta.offset;
        dir_table_size = (sav->fs_info.dirtable_info.dupmeta.count + 2) * sizeof(SaveDirectoryEntry);
        
        file_table_offset = sav->fs_info.filetable_info.dupmeta.offset;
        file_table_size = (sav->fs_info.filetable_info.dupmeta.count + 1) * sizeof(SaveFileEntry);
        
        // duplicate meta has the dir and file tables outside the data region
        u64 part_b_size = fvx_size(&part_b);

        u64 min_part_a_size = align(full_header_size +
                                    sav->fs_info.dir_hashtbl.count * sizeof(u32) +
                                    sav->fs_info.file_hashtbl.count * sizeof(u32) +
                                    (sav->fs_info.fat.count + 1) * sizeof(SaveFatEntry) +
                                    dir_table_size +
                                    file_table_size,
                                    sav->pre_header.fs_image_blocksize);
        u64 min_part_b_size = sav->fs_info.data_region.count * sav->fs_info.data_region_blocksize;
                              
        if (part_a_size < min_part_a_size || part_b_size < min_part_b_size)
            return 1;
    }

    if (!dir_table_offset || !dir_table_size || !file_table_offset || !file_table_size)
        goto err_exit;

    sav->max_num_file_entries = file_table_size / sizeof(SaveFileEntry);
    sav->max_num_dir_entries = dir_table_size / sizeof(SaveDirectoryEntry);

    if (read_alloc_chunk((void **)&sav->dir_hashtbl, &part_a, sav->fs_info.dir_hashtbl.offset, sav->fs_info.dir_hashtbl.count * 4) ||
        read_alloc_chunk((void **)&sav->file_hashtbl, &part_a, sav->fs_info.file_hashtbl.offset, sav->fs_info.file_hashtbl.count * 4) ||
        read_alloc_chunk((void **)&sav->fat_entries, &part_a, sav->fs_info.fat.offset, (sav->fs_info.fat.count + 1) * sizeof(SaveFatEntry)) ||
        read_alloc_chunk((void **)&sav->dir_entries, &part_a, dir_table_offset, dir_table_size) ||
        read_alloc_chunk((void **)&sav->file_entries, &part_a, file_table_offset, file_table_size)) {
        goto err_exit;
    }

    sav->init_ok = true;
ret:
    fvx_close(&part_a);
    fvx_close(&part_b);
    sav->init_ok = !retval;
    return retval;
err_exit:
    retval = 1;
    goto ret;
}

void SaveExsvFileFree(SaveExsvFile *sav) {
    fvx_close(&part_a);
    fvx_close(&part_b);
    data_part = NULL;
    if (sav->dir_hashtbl) { free(sav->dir_hashtbl); sav->dir_hashtbl = NULL; }
    if (sav->file_hashtbl) { free(sav->file_hashtbl); sav->file_hashtbl = NULL; }
    if (sav->fat_entries) { free(sav->fat_entries); sav->fat_entries = NULL; }
    if (sav->dir_entries) { free(sav->dir_entries); sav->dir_entries = NULL; }
    if (sav->file_entries) { free(sav->file_entries); sav->file_entries = NULL; }
    sav->max_num_file_entries = sav->max_num_dir_entries = 0;
    sav->init_ok = false;
    sav->is_exsv = false;
}

typedef struct ReadFileData {
    u32 offset;
    u32 size;
    char *outbuf;
} ReadFileData;

static int ReadFile(u32 part_offset, u32 file_offset, u32 size, void *arg) {
    ReadFileData *data = (ReadFileData *)arg;

    UINT nread = 0;

    if (file_offset <= data->offset && data->offset < file_offset + size) {
        u32 in_block_offs = data->offset - file_offset;
        u32 in_block_size = min(size - in_block_offs, data->size);

        if (fvx_lseek(data_part, part_offset + in_block_offs) != FR_OK ||
            fvx_read(data_part, data->outbuf, in_block_size, &nread) != FR_OK || nread != in_block_size) {
            return 1;
        }

        data->offset += in_block_size;
        data->outbuf += in_block_size;
        data->size -= in_block_size;
    }

    if (!data->size) return 2;

    return 0;
}

// 0: OK/continue
// 1: error
// 2: OK, early exit processing chain
typedef int (* follow_cb)(u32 in_part_offset, u32 file_offset, u32 size, void *arg);

static int ProcessFatDataBlock(SaveExsvFile *save, u32 index, u32 *file_offset, follow_cb cb, void *cb_arg) {
    if (!index)
        return 0; // not yet

    int cb_ret = cb(save->fs_info.data_region.offset + (index - 1) * save->fs_info.data_region_blocksize, *file_offset, save->fs_info.data_region_blocksize, cb_arg);
    if (cb_ret)
        return cb_ret;

    *file_offset += save->fs_info.data_region_blocksize;
    return 0;
}

static int ProcessFatChain(SaveExsvFile *save, u32 initial_index, follow_cb cb, void *arg) {
    u32 idx = initial_index;
    u32 file_offset = 0;
    int ret = 0;

    do {
        if (idx >= save->fs_info.fat.count) // shouldn't happen
            return 1;
        
        SaveFatEntry *ent = &save->fat_entries[idx];

        ret = ProcessFatDataBlock(save, idx, &file_offset, cb, arg);
        if      (ret == 2) return 0;
        else if (ret == 1) return 1;

        /* process extended nodes first */
        if (ent->V.flag) {
            if (idx + 1 >= save->fs_info.fat.count) // same as above, shouldn't happen, but still
                return 1;
            u32 end_index = save->fat_entries[idx + 1].V.index;
            for (u32 i = idx + 1; i < end_index + 1; i++) {
                ret = ProcessFatDataBlock(save, i, &file_offset, cb, arg);
                if      (ret == 2) return 0;
                else if (ret == 1) return 1;
            }
        }

        if (ent->V.index)
            idx = ent->V.index; /* go to next node, if we have one. */
        else
            idx = 0; /* no next node, we're done */
    }
    while (idx);

    return 0;
}

int SaveExsvReadFatFile(SaveExsvFile *sav, u32 index, void *buffer, u32 offset, u32 count) {
    if (!sav->init_ok || index >= sav->max_num_file_entries)
        return 1;

    if (sav->is_exsv) {
        return 0;
    } else {
        SaveFileEntry *fent = &sav->file_entries[index];
    
        // empty file
        if (fent->ent.first_block_index == 0x80000000)
            return 1;
    
        ReadFileData data = { .offset = offset, .size = count, .outbuf = (char *)buffer };
    
        return ProcessFatChain(sav, fent->ent.first_block_index + 1, ReadFile, &data);
    }
}