#include "disadiff.h"
#include "image.h"
#include "vff.h"
#include "sha.h"

#define GET_DPFS_BIT(b, lvl) (((((u32*) (void*) lvl)[b >> 5]) >> (31 - (b % 32))) & 1)
#define LVL(h,n) ((h)->level[(n) - 1])
#define L(n) ((n) - 1)

typedef struct {
    u8  magic[8]; // "DISA" 0x00040000
    u32 n_partitions;
    u8  padding0[4];
    u64 offset_table1;
    u64 offset_table0;
    u64 size_table;
    u64 offset_descA;
    u64 size_descA;
    u64 offset_descB;
    u64 size_descB;
    u64 offset_partitionA;
    u64 size_partitionA;
    u64 offset_partitionB;
    u64 size_partitionB;
    u8  active_table; // 0 or 1
    u8  padding1[3];
    u8  hash_table[0x20]; // for the active table
    u8  unused[0x74];
} PACKED_STRUCT DisaHeader;

typedef struct {
    u8  magic[8]; // "DIFF" 0x00030000
    u64 offset_table1; // also desc offset
    u64 offset_table0; // also desc offset
    u64 size_table; // includes desc size
    u64 offset_partition;
    u64 size_partition;
    u32 active_table; // 0 or 1
    u8  hash_table[0x20]; // for the active table
    u64 unique_id; // see: http://3dbrew.org/wiki/Extdata
    u8  unused[0xA4];
} PACKED_STRUCT DiffHeader;

typedef struct {
    u8  magic[8]; // "DIFI" 0x00010000
    u64 offset_ivfc; // always 0x44
    u64 size_ivfc; // always 0x78
    u64 offset_dpfs; // always 0xBC
    u64 size_dpfs; // always 0x50
    u64 offset_hash; // always 0x10C
    u64 size_hash; // may include padding
    u8  ivfc_use_extlvl4;
    u8  dpfs_lvl1_selector;
    u8  padding[2];
    u64 ivfc_offset_extlvl4;
} PACKED_STRUCT DifiHeader;

typedef struct {
    u64 offset;
    u64 size;
    u32 blocksize_log2;
    u8  padding0[4];
} PACKED_STRUCT IvfcLevel;

typedef struct {
    u8        magic[8]; // "IVFC" 0x00020000
    u32       size_hash;// same as the one in DIFI
    u8        padding0[4];
    IvfcLevel level[4];
    u32       size_ivfc;
    u32       offset_tree;
} PACKED_STRUCT IvfcDescriptor;

// same layout
typedef IvfcLevel DpfsLevel;

typedef struct {
    u8        magic[8]; // "DPFS" 0x00010000
    DpfsLevel level[3];
} PACKED_STRUCT DpfsDescriptor;

typedef struct {
    DifiHeader difi;
    IvfcDescriptor ivfc;
    DpfsDescriptor dpfs;
    u8 hash[0x20];
    u8 padding[4]; // all zeroes when encrypted
} PACKED_STRUCT DifiStruct;

STATIC_ASSERT(sizeof(DifiHeader) == 0x44);
STATIC_ASSERT(sizeof(IvfcDescriptor) == 0x78);
STATIC_ASSERT(sizeof(DpfsDescriptor) == 0x50);
STATIC_ASSERT(sizeof(DiffHeader) == 0x100);

static const u8 disa_magic[] = { DISA_MAGIC };
static const u8 diff_magic[] = { DIFF_MAGIC };
static const u8 ivfc_magic[] = { IVFC_MAGIC };
static const u8 dpfs_magic[] = { DPFS_MAGIC };
static const u8 difi_magic[] = { DIFI_MAGIC };

typedef struct {
    u32 ivfc_blocksizes[4];
    u32 dpfs_lv2_blocksize;
    u32 dpfs_lv3_blocksize;
} IvfcDpfsConfig;

static const IvfcDpfsConfig SaveExsvIvfcDpfsConfig = {
    .ivfc_blocksizes = { 0x200, 0x200, 0x1000, 0x1000 },
    .dpfs_lv2_blocksize = 0x80,
    .dpfs_lv3_blocksize = 0x1000,
};

static const IvfcDpfsConfig DbIvfcDpfsConfig = {
    .ivfc_blocksizes = { 0x200, 0x200, 0x200, 0x200 },
    .dpfs_lv2_blocksize = 0x80,
    .dpfs_lv3_blocksize = 0x200,
};

const IvfcDpfsConfig *GetIvfcDpfsConfig(bool db) {
    return db ? &DbIvfcDpfsConfig : &SaveExsvIvfcDpfsConfig;
}

u32 CalcIvfcDpfsConfigBlockSize(const IvfcDpfsConfig *config) {
    // calculates the largest block size across both IVFC and DPFS
    u32 ret = 0;

    for (int i = 1; i < 4 + 1; i++)
        ret |= (config->ivfc_blocksizes[L(i)] - 1);

    ret |= config->dpfs_lv2_blocksize - 1;
    ret |= config->dpfs_lv3_blocksize - 1;

    return ret + 1;
}

static FIL ddfile;
static FIL* ddfp = NULL;

inline static u32 DisaDiffSize(const TCHAR* path) {
    return path ? fvx_qsize(path) : GetMountSize();
}

inline static FRESULT DisaDiffOpen(const TCHAR* path) {
    FRESULT res = FR_OK;

    ddfp = NULL;
    if (path) {
        res = fvx_open(&ddfile, path, FA_READ | FA_WRITE | FA_OPEN_EXISTING);
        if (res == FR_OK) ddfp = &ddfile;
    } else if (!GetMountState()) res = FR_DENIED;

    return res;
}

inline static FRESULT DisaDiffRead(void* buf, UINT btr, UINT ofs) {
    if (ddfp) {
        FRESULT res;
        UINT br;
        if ((fvx_tell(ddfp) != ofs) &&
            (fvx_lseek(ddfp, ofs) != FR_OK)) return FR_DENIED;
        res = fvx_read(ddfp, buf, btr, &br);
        if ((res == FR_OK) && (br != btr)) res = FR_DENIED;
        return res;
    } else return (ReadImageBytes(buf, (u64) ofs, (u64) btr) == 0) ? FR_OK : FR_DENIED;
}

inline static FRESULT DisaDiffWrite(const void* buf, UINT btw, UINT ofs) {
    if (ddfp) {
        FRESULT res;
        UINT bw;
        if ((fvx_tell(ddfp) != ofs) &&
            (fvx_lseek(ddfp, ofs) != FR_OK)) return FR_DENIED;
        res = fvx_write(ddfp, buf, btw, &bw);
        if ((res == FR_OK) && (bw != btw)) res = FR_DENIED;
        return res;
    } else return (WriteImageBytes(buf, (u64) ofs, (u64) btw) == 0) ? FR_OK : FR_DENIED;
}

inline static FRESULT DisaDiffClose() {
    if (ddfp) {
        ddfp = NULL;
        return fvx_close(&ddfile);
    } else return FR_OK;
}

inline static FRESULT DisaDiffQRead(const TCHAR* path, void* buf, UINT ofs, UINT btr) {
    if (path) return fvx_qread(path, buf, ofs, btr, NULL);
    else return (ReadImageBytes(buf, (u64) ofs, (u64) btr) == 0) ? FR_OK : FR_DENIED;
}

inline static FRESULT DisaDiffQWrite(const TCHAR* path, const void* buf, UINT ofs, UINT btw) {
    if (path) return fvx_qwrite(path, buf, ofs, btw, NULL);
    else return (WriteImageBytes(buf, (u64) ofs, (u64) btw) == 0) ? FR_OK : FR_DENIED;
}

u32 GetDisaDiffRWInfo(const char* path, DisaDiffRWInfo* info, bool partitionB) {
    // reset reader info
    memset(info, 0x00, sizeof(DisaDiffRWInfo));

    // get file size, header at header offset
    u32 file_size = DisaDiffSize(path);
    u8 header[0x100];
    if (DisaDiffQRead(path, header, 0x100, 0x100) != FR_OK)
        return 1;

    // DISA/DIFF header: find partition offset & size and DIFI descriptor
    u32 offset_partition = 0;
    u32 size_partition = 0;
    u32 offset_difi = 0;
    if (memcmp(header, disa_magic, 8) == 0) { // DISA file
        DisaHeader* disa = (DisaHeader*) (void*) header;
        info->offset_table = (disa->active_table) ? disa->offset_table1 : disa->offset_table0;
        info->size_table = disa->size_table;
        offset_difi = info->offset_table;
        info->offset_partition_hash = 0x16C;
        if (!partitionB) {
            offset_partition = (u32) disa->offset_partitionA;
            size_partition = (u32) disa->size_partitionA;
            offset_difi += (u32) disa->offset_descA;
        } else {
            if (disa->n_partitions != 2) return 1;
            offset_partition = (u32) disa->offset_partitionB;
            size_partition = (u32) disa->size_partitionB;
            offset_difi += (u32) disa->offset_descB;
        }
    } else if (memcmp(header, diff_magic, 8) == 0) { // DIFF file
        if (partitionB)
            return 1;
        DiffHeader* diff = (DiffHeader*) (void*) header;
        offset_partition = (u32) diff->offset_partition;
        size_partition = (u32) diff->size_partition;
        info->unique_id = diff->unique_id;
        info->offset_table = (diff->active_table) ? diff->offset_table1 : diff->offset_table0;
        info->size_table = diff->size_table;
        offset_difi = info->offset_table;
        info->offset_partition_hash = 0x134;
    } else {
        return 1;
    }

    // check the output so far
    if (!offset_difi || (offset_difi + sizeof(DifiStruct) > file_size) || (offset_partition + size_partition > file_size))
        return 1;

    info->offset_difi = offset_difi;
    // read DIFI struct from file
    const DifiStruct difis;
    if (DisaDiffQRead(path, (DifiStruct*) &difis, offset_difi, sizeof(DifiStruct)) != FR_OK)
        return 1;

    if ((memcmp(difis.difi.magic, difi_magic, 8) != 0) ||
        (memcmp(difis.ivfc.magic, ivfc_magic, 8) != 0) ||
        (memcmp(difis.dpfs.magic, dpfs_magic, 8) != 0))
        return 1;

    // check & get data from DIFI header
    const DifiHeader* difi = &(difis.difi);
    if ((difi->offset_ivfc != sizeof(DifiHeader)) ||
        (difi->size_ivfc != sizeof(IvfcDescriptor)) ||
        (difi->offset_dpfs != difi->offset_ivfc + difi->size_ivfc) ||
        (difi->size_dpfs != sizeof(DpfsDescriptor)) ||
        (difi->offset_hash != difi->offset_dpfs + difi->size_dpfs) ||
        (difi->size_hash < 0x20))
        return 1;

    info->dpfs_lvl1_selector = difi->dpfs_lvl1_selector;
    info->ivfc_use_extlvl4 = difi->ivfc_use_extlvl4;
    info->offset_ivfc_lvl4 = (u32) (offset_partition + difi->ivfc_offset_extlvl4);
    info->offset_master_hash = (u32) difi->offset_hash;

    // check & get data from DPFS descriptor
    const DpfsDescriptor* dpfs = &(difis.dpfs);
    if ((LVL(dpfs, 1).offset + LVL(dpfs, 1).size > LVL(dpfs, 2).offset) ||
        (LVL(dpfs, 2).offset + LVL(dpfs, 2).size > LVL(dpfs, 3).offset) ||
        (LVL(dpfs, 3).offset + LVL(dpfs, 3).size > size_partition) ||
        (2 > LVL(dpfs, 2).blocksize_log2) || (LVL(dpfs, 2).blocksize_log2 > LVL(dpfs, 3).blocksize_log2) ||
        !LVL(dpfs, 1).size || !LVL(dpfs, 2).size || !LVL(dpfs, 3).size)
        return 1;

    info->offset_dpfs_lvl1 = (u32) (offset_partition + LVL(dpfs, 1).offset);
    info->offset_dpfs_lvl2 = (u32) (offset_partition + LVL(dpfs, 2).offset);
    info->offset_dpfs_lvl3 = (u32) (offset_partition + LVL(dpfs, 3).offset);
    info->size_dpfs_lvl1 = (u32) LVL(dpfs, 1).size;
    info->size_dpfs_lvl2 = (u32) LVL(dpfs, 2).size;
    info->size_dpfs_lvl3 = (u32) LVL(dpfs, 3).size;
    info->log_dpfs_lvl2 = (u32) LVL(dpfs, 2).blocksize_log2;
    info->log_dpfs_lvl3 = (u32) LVL(dpfs, 3).blocksize_log2;

    // check & get data from IVFC descriptor
    const IvfcDescriptor* ivfc = &(difis.ivfc);
    if ((ivfc->size_hash != difi->size_hash) ||
        (ivfc->size_ivfc != sizeof(IvfcDescriptor)) ||
        (ivfc->offset_tree != 0) ||
        (LVL(ivfc, 1).offset + LVL(ivfc, 1).size > LVL(ivfc, 2).offset) ||
        (LVL(ivfc, 2).offset + LVL(ivfc, 2).size > LVL(ivfc, 3).offset) ||
        (LVL(ivfc, 3).offset + LVL(ivfc, 3).size > LVL(dpfs, 3).size))
        return 1;

    if (!info->ivfc_use_extlvl4) {
        if ((LVL(ivfc, 3).offset + LVL(ivfc, 3).size > LVL(ivfc, 4).offset) ||
            (LVL(ivfc, 4).offset + LVL(ivfc, 4).size > LVL(dpfs, 3).size))
            return 1;

        info->offset_ivfc_lvl4 = (u32) LVL(ivfc, 4).offset;
    } else if (info->offset_ivfc_lvl4 + LVL(ivfc, 4).size > offset_partition + size_partition)
        return 1;

    info->log_ivfc_lvl1 = (u32) LVL(ivfc, 1).blocksize_log2;
    info->log_ivfc_lvl2 = (u32) LVL(ivfc, 2).blocksize_log2;
    info->log_ivfc_lvl3 = (u32) LVL(ivfc, 3).blocksize_log2;
    info->log_ivfc_lvl4 = (u32) LVL(ivfc, 4).blocksize_log2;
    info->offset_ivfc_lvl1 = (u32) LVL(ivfc, 1).offset;
    info->offset_ivfc_lvl2 = (u32) LVL(ivfc, 2).offset;
    info->offset_ivfc_lvl3 = (u32) LVL(ivfc, 3).offset;
    info->size_ivfc_lvl1 = (u32) LVL(ivfc, 1).size;
    info->size_ivfc_lvl2 = (u32) LVL(ivfc, 2).size;
    info->size_ivfc_lvl3 = (u32) LVL(ivfc, 3).size;
    info->size_ivfc_lvl4 = (u32) LVL(ivfc, 4).size;

    return 0;
}

u32 BuildDisaDiffDpfsLvl2Cache(const char* path, const DisaDiffRWInfo* info, u8* cache, u32 cache_size) {
    const u32 blocksize_lvl2 = 1u << info->log_dpfs_lvl2;
    const u32 blocksize_lvl3 = 1u << info->log_dpfs_lvl3;

    // each lvl3 block maps to exactly one bit in lvl2
    const u32 lv2_min_num_bits = ceil_div(info->size_dpfs_lvl3, blocksize_lvl3);
    // the number of bits in lvl2 are rounded up to a byte-boundary, 8 bits
    const u32 lv2_min_num_bytes = ceil_div(lv2_min_num_bits, 8);
    // and the number of bytes lvl2 consists of is rounded up to the lvl2 blocksize
    // so that each lvl2 block maps to exactly one bit of lvl1, respectively
    const u32 lv2_min_num_blocks = ceil_div(lv2_min_num_bytes, blocksize_lvl2);
    // thus, the minimum size of lvl2 is the lvl2-blockaligned number of lvl2 bytes (which itself are the 8-bit aligned number of lvl3 blocks)
    const u32 lv2_min_size = lv2_min_num_blocks * blocksize_lvl2;

    const u32 offset_lvl1 = info->offset_dpfs_lvl1 + ((info->dpfs_lvl1_selector) ? info->size_dpfs_lvl1 : 0);

    // safety (this still assumes all the checks from GetDisaDiffRWInfo())
    if ((cache_size < lv2_min_size) ||
        (lv2_min_size > info->size_dpfs_lvl2) ||
        (lv2_min_size > (info->size_dpfs_lvl1 << (3 + info->log_dpfs_lvl2)))) {
        return 1;
    }

    // allocate memory
    u8* lvl1 = (u8*) malloc(info->size_dpfs_lvl1);
    if (!lvl1) return 1;

    // open file pointer
    if (DisaDiffOpen(path) != FR_OK) {
        free(lvl1);
        return 1;
    }

    // read lvl1
    u32 ret = 0;
    if ((ret != 0) || DisaDiffRead(lvl1, info->size_dpfs_lvl1, offset_lvl1)) ret = 1;

    // read full lvl2_0 to cache. this is the baseline, and we'll replace blocks that are actually in lv2_1 later
    if ((ret != 0) || DisaDiffRead(cache, info->size_dpfs_lvl2, info->offset_dpfs_lvl2)) ret = 1;

    u32 offset_lvl2_1 = info->offset_dpfs_lvl2 + info->size_dpfs_lvl2;

    for (u32 i = 0; i < lv2_min_num_blocks; i++) {
        if (!GET_DPFS_BIT(i, lvl1)) {
            // this lv2 block is not in lv2_1, so we don't need to replace it
            continue;
        }

        u32 offset = i * blocksize_lvl2;
        if (offset > cache_size || offset + blocksize_lvl2 > cache_size || blocksize_lvl2 > cache_size) {
            // this was known to corrupt the heap before, and shouldn't ever happen, but still
            ret = 1;
            break;
        }

        if (DisaDiffRead((u8*) cache + offset, blocksize_lvl2, offset_lvl2_1 + offset) != FR_OK) {
            ret = 1;
            break;
        }
    }

    ((DisaDiffRWInfo*) info)->dpfs_lvl2_cache = cache;
    free(lvl1);
    DisaDiffClose();
    return ret;
}

static u32 ReadDisaDiffDpfsLvl3(const DisaDiffRWInfo* info, u32 offset, u32 size, void* buffer) { // assumes file is already open
    const u32 offset_start = offset;
    const u32 offset_end = offset_start + size;
    const u8* lvl2 = info->dpfs_lvl2_cache;
    const u32 offset_lvl3_0 = info->offset_dpfs_lvl3;
    const u32 offset_lvl3_1 = offset_lvl3_0 + info->size_dpfs_lvl3;
    const u32 log_lvl3 = info->log_dpfs_lvl3;

    u32 read_start = offset_start;
    u32 read_end = read_start;
    u32 bit_state = 0;

    // full reading below
    while (size && (read_start < offset_end)) {
        // read bits until bit_state does not match
        // idx_lvl2 is a bit offset
        u32 idx_lvl2 = read_end >> log_lvl3;
        if (GET_DPFS_BIT(idx_lvl2, lvl2) == bit_state) {
            read_end = (idx_lvl2+1) << log_lvl3;
            if (read_end >= offset_end) read_end = offset_end;
            else continue;
        }
        // read data if there is any
        if (read_start < read_end) {
            const u32 pos_f = (bit_state ? offset_lvl3_1 : offset_lvl3_0) + read_start;
            const u32 pos_b = read_start - offset_start;
            const u32 btr = read_end - read_start;
            if (DisaDiffRead(((u8*) buffer) + pos_b, btr, pos_f) != FR_OK) size = 0;
            read_start = read_end;
        }
        // flip the bit_state
        bit_state = ~bit_state & 0x1;
    }

    return size;
}

static u32 WriteDisaDiffDpfsLvl3(const DisaDiffRWInfo* info, u32 offset, u32 size, const void* buffer) { // assumes file is already open, does not fix hashes
    const u32 offset_start = offset;
    const u32 offset_end = offset_start + size;
    const u8* lvl2 = info->dpfs_lvl2_cache;
    const u32 offset_lvl3_0 = info->offset_dpfs_lvl3;
    const u32 offset_lvl3_1 = offset_lvl3_0 + info->size_dpfs_lvl3;
    const u32 log_lvl3 = info->log_dpfs_lvl3;

    u32 write_start = offset_start;
    u32 write_end = write_start;
    u32 bit_state = 0;

    // full reading below
    while (size && (write_start < offset_end)) {
        // write bits until bit_state does not match
        // idx_lvl2 is a bit offset
        u32 idx_lvl2 = write_end >> log_lvl3;
        if (GET_DPFS_BIT(idx_lvl2, lvl2) == bit_state) {
            write_end = (idx_lvl2+1) << log_lvl3;
            if (write_end >= offset_end) write_end = offset_end;
            else continue;
        }
        // write data if there is any
        if (write_start < write_end) {
            const u32 pos_f = (bit_state ? offset_lvl3_1 : offset_lvl3_0) + write_start;
            const u32 pos_b = write_start - offset_start;
            const u32 btw = write_end - write_start;
            if (DisaDiffWrite(((u8*) buffer) + pos_b, btw, pos_f) != FR_OK) size = 0;
            write_start = write_end;
        }
        // flip the bit_state
        bit_state = ~bit_state & 0x1;
    }

    return size;
}

u32 FixDisaDiffPartitionHash(const DisaDiffRWInfo* info) {
    const u32 size = info->size_table;
    u8 sha_buf[0x20];
    u8* buf;

    if (!(buf = malloc(size)))
        return 1;

    if (DisaDiffRead(buf, size, info->offset_table) != FR_OK) {
        free(buf);
        return 1;
    }

    sha_quick(sha_buf, buf, size, SHA256_MODE);

    free(buf);

    if (DisaDiffWrite(sha_buf, 0x20, info->offset_partition_hash) != FR_OK)
        return 1;

    return 0;
}

u32 FixDisaDiffIvfcLevel(const DisaDiffRWInfo* info, u32 level, u32 offset, u32 size, u32* next_offset, u32* next_size) {
    if (level == 0)
        return FixDisaDiffPartitionHash(info);

    if (level > 4)
        return 1;

    const u32 offset_ivfc_lvl = (&(info->offset_ivfc_lvl1))[level - 1];
    const u32 size_ivfc_lvl = (&(info->size_ivfc_lvl1))[level - 1];
    const u32 log_ivfc_lvl = (&(info->log_ivfc_lvl1))[level - 1];
    const u32 block_size = 1 << log_ivfc_lvl;
    u32 read_size = block_size;
    u32 align_offset = (offset >> log_ivfc_lvl) << log_ivfc_lvl; // align starting offset
    u32 align_size = size + offset - align_offset; // increase size by the amount starting offset decreased when aligned

    if (level != 1) {
        if (next_offset) *next_offset = (align_offset >> log_ivfc_lvl) * 0x20;
        if (next_size) *next_size = ((align_size >> log_ivfc_lvl) + (((align_size % block_size) == 0) ? 0 : 1)) * 0x20;
    }

    u8 sha_buf[0x20];
    u8* buf;

    if (!(buf = malloc(block_size)))
        return 1;

    while (align_size > 0) {
        if (align_offset + block_size > size_ivfc_lvl) {
            memset(buf, 0, block_size);
            read_size -= (align_offset + block_size - size_ivfc_lvl);
        }

        if (((level == 4) && info->ivfc_use_extlvl4) ? (DisaDiffRead(buf, read_size, align_offset + offset_ivfc_lvl) != FR_OK) :
            (ReadDisaDiffDpfsLvl3(info, align_offset + offset_ivfc_lvl, read_size, buf) != read_size)) {
            free(buf);
            return 1;
        }

        sha_quick(sha_buf, buf, block_size, SHA256_MODE);


        if ((level == 1) ? (DisaDiffWrite(sha_buf, 0x20, info->offset_difi + info->offset_master_hash + ((align_offset >> log_ivfc_lvl) * 0x20)) != FR_OK) :
            (WriteDisaDiffDpfsLvl3(info, (&(info->offset_ivfc_lvl1))[level - 2] + ((align_offset >> log_ivfc_lvl) * 0x20), 0x20, sha_buf) != 0x20)) {
            free(buf);
            return 1;
        }

        align_offset += block_size;
        align_size = ((align_size < block_size) ? 0 : (align_size - block_size));
    }

    free(buf);

    return 0;
}

u32 ReadDisaDiffIvfcLvl4(const char* path, const DisaDiffRWInfo* info, u32 offset, u32 size, void* buffer) { // offset: offset inside IVFC lvl4
    // DisaDiffRWInfo not provided?
    DisaDiffRWInfo info_l;
    u8* cache = NULL;
    if (!info) {
        info = &info_l;
        if (GetDisaDiffRWInfo(path, (DisaDiffRWInfo*) info, false) != 0) return 0;
        cache = malloc(info->size_dpfs_lvl2);
        if (!cache) return 0;
        if (BuildDisaDiffDpfsLvl2Cache(path, info, cache, info->size_dpfs_lvl2) != 0) {
            free(cache);
            return 0;
        }
    }

    // open file pointer
    if (DisaDiffOpen(path) != FR_OK)
        size = 0;

    // sanity checks - offset & size
    if (offset > info->size_ivfc_lvl4) return 0;
    else if (offset + size > info->size_ivfc_lvl4) size = info->size_ivfc_lvl4 - offset;

    if (info->ivfc_use_extlvl4) {
        if (DisaDiffRead(buffer, size, info->offset_ivfc_lvl4 + offset) != FR_OK)
            size = 0;
    } else {
        size = ReadDisaDiffDpfsLvl3(info, info->offset_ivfc_lvl4 + offset, size, buffer);
    }

    DisaDiffClose();
    if (cache) free(cache);
    return size;
}

u32 WriteDisaDiffIvfcLvl4(const char* path, const DisaDiffRWInfo* info, u32 offset, u32 size, const void* buffer) { // offset: offset inside IVFC lvl4. cmac still needs fixed after calling this.
    // DisaDiffRWInfo not provided?
    DisaDiffRWInfo info_l;
    u8* cache = NULL;
    if (!info) {
        info = &info_l;
        if (GetDisaDiffRWInfo(path, (DisaDiffRWInfo*) info, false) != 0)
            return 0;
        cache = malloc(info->size_dpfs_lvl2);
        if (!cache)
            return 0;
        if (BuildDisaDiffDpfsLvl2Cache(path, info, cache, info->size_dpfs_lvl2) != 0) {
            free(cache);
            return 0;
        }
    }

    // sanity check - offset & size
    if (offset + size > info->size_ivfc_lvl4)
        return 0;

    // open file pointer
    if (DisaDiffOpen(path) != FR_OK)
        size = 0;

    if (info->ivfc_use_extlvl4) {
        if (DisaDiffWrite(buffer, size, info->offset_ivfc_lvl4 + offset) != FR_OK)
            size = 0;
    } else {
        size = WriteDisaDiffDpfsLvl3(info, info->offset_ivfc_lvl4 + offset, size, buffer);
    }

    if ((size != 0) && ddfp) { // if we're writing to a mounted image, the hash chain will be handled later by vdisadiff
        u32 hashfix_offset = offset, hashfix_size = size;
        for (int i = 4; i >= 0; i--) {
            if (FixDisaDiffIvfcLevel(info, i, hashfix_offset, hashfix_size, &hashfix_offset, &hashfix_size) != 0) {
                size = 0;
                break;
            }
        }
    }

    DisaDiffClose();
    if (cache) free(cache);
    return size;
}

static inline u64 CalcIvfcTreeSize(const IvfcDescriptor *ivfc, bool ext_lv4) {
    int end_level = ext_lv4 ? 3 : 4;
    return LVL(ivfc, end_level).offset + LVL(ivfc, end_level).size - LVL(ivfc, 1).offset;
}

static inline u64 CalcDpfsTreeSize(const DpfsDescriptor *dpfs) {
    return LVL(dpfs, 3).offset + LVL(dpfs, 3).size * 2;
}

static inline u64 CalcDifiDescriptorSize(const DifiHeader *difi) {
    return difi->offset_hash + difi->size_hash;
}

static inline u64 CalcDiffFileSize(const DiffHeader *diff) {
    return diff->offset_partition + diff->size_partition;
}

static void BuildIvfcDescriptor(IvfcDescriptor *ivfc, u64 data_size, bool db) {
    const IvfcDpfsConfig *config = GetIvfcDpfsConfig(db);

    memset(ivfc, 0, sizeof(IvfcDescriptor));
    memcpy(ivfc, ivfc_magic, sizeof(ivfc_magic));
    ivfc->size_ivfc = sizeof(IvfcDescriptor);
    ivfc->offset_tree = 0;

    LVL(ivfc, 4).size = data_size;
    LVL(ivfc, 3).size = 32 * ceil_div(data_size, config->ivfc_blocksizes[L(4)]);
    LVL(ivfc, 2).size = 32 * ceil_div(LVL(ivfc, 3).size, config->ivfc_blocksizes[L(3)]);
    LVL(ivfc, 1).size = 32 * ceil_div(LVL(ivfc, 2).size, config->ivfc_blocksizes[L(2)]);

    ivfc->size_hash = 32 * ceil_div(LVL(ivfc, 1).size, config->ivfc_blocksizes[L(1)]);

    u64 cur_offs = 0;

    for (int i = 1; i < 4 + 1; i++) {
        u64 cur_lvl_offs = 0;
        if (LVL(ivfc, i).size < 4 * config->ivfc_blocksizes[L(i)]) {
            cur_lvl_offs = align_pow2(cur_offs, 8);
        } else {
            cur_lvl_offs = align_pow2(cur_offs, config->ivfc_blocksizes[L(i)]);
        }

        LVL(ivfc, i).offset = cur_lvl_offs;
        LVL(ivfc, i).blocksize_log2 = log_2(config->ivfc_blocksizes[L(i)]);

        cur_offs = LVL(ivfc, i).offset + LVL(ivfc, i).size;
    }
}

static void BuildDpfsDescriptor(DpfsDescriptor *dpfs, u64 data_dupsize, bool db) {
    const IvfcDpfsConfig *config = GetIvfcDpfsConfig(db);

    memset(dpfs, 0, sizeof(DpfsDescriptor));
    memcpy(dpfs, dpfs_magic, sizeof(dpfs_magic));

    u32 lv2_num_block = ceil_div(data_dupsize, config->dpfs_lv3_blocksize);
    u32 lv2_num_byte  = ceil_div(lv2_num_block, 8);
    u32 lv2_size      = align_pow2(lv2_num_byte, config->dpfs_lv2_blocksize);

    u32 lv1_num_block = ceil_div(lv2_size, config->dpfs_lv2_blocksize);
    u32 lv1_num_byte  = ceil_div(lv1_num_block, 8);
    u32 lv1_size      = align_pow2(lv1_num_byte, 4);

    LVL(dpfs, 1).offset = 0;
    LVL(dpfs, 1).size = lv1_size;
    LVL(dpfs, 1).blocksize_log2 = 0;

    LVL(dpfs, 2).offset = lv1_size * 2;
    LVL(dpfs, 2).size = lv2_size;
    LVL(dpfs, 2).blocksize_log2 = log_2(config->dpfs_lv2_blocksize);

    LVL(dpfs, 3).offset = align_pow2(LVL(dpfs, 2).offset + lv2_size * 2, config->dpfs_lv3_blocksize);
    LVL(dpfs, 3).size = (u32)data_dupsize;
    LVL(dpfs, 3).blocksize_log2 = log_2(config->dpfs_lv3_blocksize);
}

static void BuildDifiDescriptor(DifiHeader *difi, IvfcDescriptor *ivfc, DpfsDescriptor *dpfs, u64 data_size, bool ext_lv4, bool db) {
    const IvfcDpfsConfig *config = GetIvfcDpfsConfig(db);

    BuildIvfcDescriptor(ivfc, data_size, db);

    u64 dpfs_duped_size = align_pow2(CalcIvfcTreeSize(ivfc, ext_lv4), config->dpfs_lv3_blocksize);

    BuildDpfsDescriptor(dpfs, dpfs_duped_size, db);

    memset(difi, 0, sizeof(DifiHeader));
    memcpy(difi, difi_magic, sizeof(difi_magic));
    difi->size_ivfc = sizeof(IvfcDescriptor);
    difi->size_dpfs = sizeof(DpfsDescriptor);
    difi->size_hash = ivfc->size_hash;
    difi->dpfs_lvl1_selector = 1;
    difi->ivfc_use_extlvl4 = ext_lv4 ? 1 : 0;

    difi->offset_ivfc = sizeof(DifiHeader);
    difi->offset_dpfs = align_pow2(difi->offset_ivfc + difi->size_ivfc, 4);
    difi->offset_hash = align_pow2(difi->offset_dpfs + difi->size_dpfs, 4);

    if (ext_lv4) {
        difi->ivfc_offset_extlvl4 = align_pow2(CalcDpfsTreeSize(dpfs), config->ivfc_blocksizes[L(4)]);
    } else {
        difi->ivfc_offset_extlvl4 = 0;
    }
}

static void BuildDiffHeader(DiffHeader *diff, DifiHeader *difi, IvfcDescriptor *ivfc, DpfsDescriptor *dpfs, u64 data_size, bool ext_lv4, bool db, u64 *out_uid) {
    const IvfcDpfsConfig *config = GetIvfcDpfsConfig(db);

    BuildDifiDescriptor(difi, ivfc, dpfs, data_size, ext_lv4, db);

    memset(diff, 0, sizeof(DiffHeader));

    memcpy(diff, diff_magic, sizeof(diff_magic));

    if (db) { // db files have a 0 in the unique id field
        diff->unique_id = 0;
    } else { // non-db DIFFs (mainly extdata files) have a random 8-byte unique id
        u32 *uid = (u32 *)&diff->unique_id;
        *uid++ = (u32)rand();
        *uid   = (u32)rand();
    }

    if (out_uid)
        *out_uid = diff->unique_id;

    diff->size_table = CalcDifiDescriptorSize(difi);

    if (ext_lv4) {
        diff->size_partition = difi->ivfc_offset_extlvl4 + data_size;
    } else {
        diff->size_partition = CalcDpfsTreeSize(dpfs);
    }

    diff->offset_table1 = sizeof(DiffHeader) + 0x100 /* CMAC section size */;
    diff->offset_table0 = align_pow2(diff->offset_table1 + diff->size_table, 8);
    diff->offset_partition = align_pow2(diff->offset_table0 + diff->size_table, CalcIvfcDpfsConfigBlockSize(config));
    diff->active_table = 1;
}

u64 BuildDiffCalcRequiredSize(u64 data_size, bool ext_lv4, bool db) {
    IvfcDescriptor ivfc;
    DpfsDescriptor dpfs;
    DifiHeader difi;
    DiffHeader diff;

    BuildDiffHeader(&diff, &difi, &ivfc, &dpfs, data_size, ext_lv4, db, NULL);

    return diff.offset_partition + diff.size_partition;
}

u32 CreateDiff(const char *path, u64 data_size, bool ext_lv4, bool db, u64 *out_uid) {
    IvfcDescriptor ivfc;
    DpfsDescriptor dpfs;
    DifiHeader difi;
    DiffHeader diff;
    u8 cmac[0x100];

    BuildDiffHeader(&diff, &difi, &ivfc, &dpfs, data_size, ext_lv4, db, out_uid);

    memset(cmac, 0, sizeof(cmac));

    u8 *part_desc = (u8 *)malloc(diff.size_table);
    if (!part_desc)
        return 1;
    u8 *desc = part_desc;

    memcpy(desc, &difi, sizeof(DifiHeader));     desc += sizeof(DifiHeader);
    memcpy(desc, &ivfc, sizeof(IvfcDescriptor)); desc += sizeof(IvfcDescriptor);
    memcpy(desc, &dpfs, sizeof(DpfsDescriptor)); desc += sizeof(DpfsDescriptor);
    memset(desc, 0, difi.size_hash);

    sha_quick(diff.hash_table, part_desc, diff.size_table, SHA256_MODE);

    FIL fp;
    UINT written = 0;

    if (fvx_open(&fp, path, FA_WRITE | FA_OPEN_EXISTING) != FR_OK ||
        fvx_size(&fp) < CalcDiffFileSize(&diff) ||
        (fvx_write(&fp, cmac, sizeof(cmac), &written) != FR_OK || written != sizeof(cmac)) ||
        (fvx_write(&fp, &diff, sizeof(diff), &written) != FR_OK || written != sizeof(diff)) ||
        fvx_lseek(&fp, diff.offset_table1) != FR_OK ||
        (fvx_write(&fp, part_desc, diff.size_table, &written) != FR_OK || written != diff.size_table) ||
        fvx_lseek(&fp, diff.offset_table0) != FR_OK ||
        (fvx_write(&fp, part_desc, diff.size_table, &written) != FR_OK || written != diff.size_table)) {
        free(part_desc);
        fvx_close(&fp);
        return 1;
    }

    free(part_desc);
    fvx_close(&fp);
    return 0;
}