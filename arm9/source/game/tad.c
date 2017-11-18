#include "tad.h"


u32 BuildTadContentTable(void* table, void* header) {
    TadHeader* hdr = (TadHeader*) header;
    TadContentTable* tbl = (TadContentTable*) table;
    
    if (strncmp(hdr->magic, TAD_HEADER_MAGIC, strlen(TAD_HEADER_MAGIC)) != 0)
        return 1;
    
    tbl->banner_end = 0 + sizeof(TadBanner) + sizeof(TadBlockMetaData);
    tbl->header_end = tbl->banner_end + sizeof(TadHeader) + sizeof(TadBlockMetaData);
    tbl->footer_end = tbl->header_end + sizeof(TadFooter) + sizeof(TadBlockMetaData);
    
    u32 content_end_last = tbl->footer_end;
    for (u32 i = 0; i < TAD_NUM_CONTENT; i++) {
        tbl->content_end[i] = content_end_last;
        if (!hdr->content_size[i]) continue; // non-existant section
        tbl->content_end[i] += align(hdr->content_size[i], 0x10) + sizeof(TadBlockMetaData);
        content_end_last = tbl->content_end[i];
    }
    
    return 0;
}
