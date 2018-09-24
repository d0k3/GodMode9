#include "romfs.h"
#include "utf.h"


// get lvl datablock offset from IVC (zero for total size)
// see: https://github.com/profi200/Project_CTR/blob/046bb359ee95423938886dbf477d00690aaecd3e/ctrtool/ivfc.c#L88-L111
u64 GetRomFsLvOffset(RomFsIvfcHeader* ivfc, u32 lvl) {
    // regardless of lvl given, we calculate them all
    u64 offset[4];

    // lvl1/2/3 offset and size
    offset[3] = align(sizeof(RomFsIvfcHeader) + ivfc->size_masterhash, 1 << ivfc->log_lvl3);
    offset[1] = offset[3] + align(ivfc->size_lvl3, 1 << ivfc->log_lvl3);
    offset[2] = offset[1] + align(ivfc->size_lvl1, 1 << ivfc->log_lvl1);
    offset[0] = offset[2] + align(ivfc->size_lvl2, 1 << ivfc->log_lvl2); // (min) size

    return (lvl <= 3) ? offset[lvl] : 0;
}

// validate IVFC header by checking offsets and hash sizes
u32 ValidateRomFsHeader(RomFsIvfcHeader* ivfc, u32 max_size) {
    u8 magic[] = { ROMFS_MAGIC };

    // check magic number
    if (memcmp(magic, ivfc->magic, sizeof(magic)) != 0)
        return 1;

    // check hash block sizes vs data block sizes
    if ((((ivfc->size_masterhash / 0x20) << ivfc->log_lvl1) < ivfc->size_lvl1) || // lvl1
        (((ivfc->size_lvl1 / 0x20) << ivfc->log_lvl2) < ivfc->size_lvl2) || // lvl2
        (((ivfc->size_lvl2 / 0x20) << ivfc->log_lvl3) < ivfc->size_lvl3)) // lvl3
        return 1;

    // check size if given
    if (max_size && (max_size < GetRomFsLvOffset(ivfc, 0)))
        return 1;

    return 0;
}

// validate lvl3 header by checking offsets and sizes
u32 ValidateLv3Header(RomFsLv3Header* lv3, u32 max_size) {
    return ((lv3->size_header == 0x28) &&
        (lv3->offset_dirhash  >= lv3->size_header) &&
        (lv3->offset_dirmeta  >= lv3->offset_dirhash + lv3->size_dirhash) &&
        (lv3->offset_filehash >= lv3->offset_dirmeta + lv3->size_dirmeta) &&
        (lv3->offset_filemeta >= lv3->offset_filehash + lv3->size_filehash) &&
        (lv3->offset_filedata >= lv3->offset_filemeta + lv3->size_filemeta) &&
        (!max_size || (lv3->offset_filedata <= max_size))) ? 0 : 1;
}

// build index of RomFS lvl3
u32 BuildLv3Index(RomFsLv3Index* index, u8* lv3) {
    RomFsLv3Header* hdr = (RomFsLv3Header*) lv3;
    index->header = hdr;
    index->dirhash = (u32*) (void*) (lv3 + hdr->offset_dirhash);
    index->dirmeta = lv3 + hdr->offset_dirmeta;
    index->filehash = (u32*) (void*) (lv3 + hdr->offset_filehash);
    index->filemeta = lv3 + hdr->offset_filemeta;
    index->filedata = NULL;
    
    index->mod_dir = (hdr->size_dirhash / sizeof(u32));
    index->mod_file = (hdr->size_filehash / sizeof(u32));
    index->size_dirmeta = hdr->size_dirmeta;
    index->size_filemeta = hdr->size_filemeta;
    
    return 0;
}

// hash lvl3 path - this is used to find the first offset in the file / dir hash table
u32 HashLv3Path(u16* wname, u32 name_len, u32 offset_parent) {
    u32 hash = offset_parent ^ 123456789;
    for (u32 i = 0; i < name_len; i++)
        hash = ((hash>>5) | (hash<<27)) ^ wname[i];
    return hash;
}

RomFsLv3DirMeta* GetLv3DirMeta(const char* name, u32 offset_parent, RomFsLv3Index* index) {
    RomFsLv3DirMeta* meta;
    
    // wide (UTF-16) name
    u16 wname[256];
    int name_len = utf8_to_utf16(wname, (u8*) name, 255, 255);
    if (name_len <= 0) return NULL;
    wname[name_len] = 0;
    
    // hashing, first offset
    u32 hash = HashLv3Path(wname, name_len, offset_parent);
    u32 offset = index->dirhash[hash % index->mod_dir];
    // process the hashbucket (make sure we got the correct data)
    // slim chance of endless loop with broken lvl3 here
    for (; offset < index->size_dirmeta; offset = meta->offset_samehash) {
        meta = (RomFsLv3DirMeta*) (index->dirmeta + offset);
        if ((offset_parent == meta->offset_parent) &&
            ((u32) name_len == meta->name_len / 2) &&
            (memcmp(wname, meta->wname, name_len * 2) == 0))
            break;
    }
    
    return (offset >= index->size_dirmeta) ? NULL : meta; 
}

RomFsLv3FileMeta* GetLv3FileMeta(const char* name, u32 offset_parent, RomFsLv3Index* index) {
    RomFsLv3FileMeta* meta;
    
    // wide (UTF-16) name
    u16 wname[256];
    int name_len = utf8_to_utf16(wname, (u8*) name, 255, 255);
    if (name_len <= 0) return NULL;
    wname[name_len] = 0;
    
    // hashing, first offset
    u32 hash = HashLv3Path(wname, name_len, offset_parent);
    u32 offset = index->filehash[hash % index->mod_file];
    // process the hashbucket (make sure we got the correct data)
    // slim chance of endless loop with broken lvl3 here
    for (; offset < index->size_filemeta; offset = meta->offset_samehash) {
        meta = (RomFsLv3FileMeta*) (index->filemeta + offset);
        if ((offset_parent == meta->offset_parent) &&
            ((u32) name_len == meta->name_len / 2) &&
            (memcmp(wname, meta->wname, name_len * 2) == 0))
            break;
    }
    
    return (offset >= index->size_filemeta) ? NULL : meta; 
}
