#include "ncch.h"

u32 ValidateNcchHeader(NcchHeader* header) {
    if (memcmp(header->magic, "NCCH", 4) != 0) // check magic number
        return 1;
    
    u32 ncch_units = (NCCH_EXTHDR_OFFSET + header->size_exthdr) / NCCH_MEDIA_UNIT; // exthdr
    if (header->size_plain) { // plain region
        if (header->offset_plain < ncch_units) return 1; // overlapping plain region
        ncch_units = (header->offset_plain + header->size_plain);
    }
    if (header->size_exefs) { // ExeFS
        if (header->offset_exefs < ncch_units) return 1; // overlapping exefs region
        ncch_units = (header->offset_exefs + header->size_exefs);
    }
    if (header->size_romfs) { // RomFS
        if (header->offset_romfs < ncch_units) return 1; // overlapping romfs region
        ncch_units = (header->offset_romfs + header->size_romfs);
    }
    // size check
    if (ncch_units > header->size) return 1; 
     
    return 0;
}
