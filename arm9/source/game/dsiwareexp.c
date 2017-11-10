#include "dsiwareexp.h"


u32 BuildDsiWareExportContentTable(void* table, void* header) {
    DsiWareExpHeader* hdr = (DsiWareExpHeader*) header;
    DsiWareExpContentTable* tbl = (DsiWareExpContentTable*) table;
    
    if (strncmp(hdr->magic, DSIWEXP_HEADER_MAGIC, strlen(DSIWEXP_HEADER_MAGIC)) != 0)
        return 1;
    
    tbl->banner_end = 0 + sizeof(DsiWareExpBanner) + sizeof(DsiWareExpBlockMetaData);
    tbl->header_end = tbl->banner_end + sizeof(DsiWareExpHeader) + sizeof(DsiWareExpBlockMetaData);
    tbl->footer_end = tbl->header_end + sizeof(DsiWareExpFooter) + sizeof(DsiWareExpBlockMetaData);
    
    u32 content_end_last = tbl->footer_end;
    for (u32 i = 0; i < DSIWEXP_NUM_CONTENT; i++) {
        tbl->content_end[i] = content_end_last;
        if (!hdr->content_size[i]) continue; // non-existant section
        tbl->content_end[i] += align(hdr->content_size[i], 0x10) + sizeof(DsiWareExpBlockMetaData);
        content_end_last = tbl->content_end[i];
    }
    
    return 0;
}
