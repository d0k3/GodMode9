#include "romfs.h"
#include "utf.h"

// validate header by checking offsets and sizes
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
