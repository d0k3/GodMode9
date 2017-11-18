#include "gba.h"
#include "sha.h"
#include "sdmmc.h"

#define AGBLOGO_SHA256 \
    0x08, 0xA0, 0x15, 0x3C, 0xFD, 0x6B, 0x0E, 0xA5, 0x4B, 0x93, 0x8F, 0x7D, 0x20, 0x99, 0x33, 0xFA, \
    0x84, 0x9D, 0xA0, 0xD5, 0x6F, 0x5A, 0x34, 0xC4, 0x81, 0x06, 0x0C, 0x9F, 0xF2, 0xFA, 0xD8, 0x18

u32 ValidateAgbSaveHeader(AgbSaveHeader* header) {
    u8 magic[] = { AGBSAVE_MAGIC };
    
    // basic checks
    if ((memcmp(header->magic, magic, sizeof(magic)) != 0) ||
        (header->unknown0 != 1) || (header->save_start != 0x200) ||
        (header->save_size > AGBSAVE_MAX_SSIZE) || !(GBASAVE_VALID(header->save_size)))
        return 1;
        
    // reserved area checks
    for (u32 i = 0; i < sizeof(header->reserved0); i++) if (header->reserved0[i] != 0xFF) return 1;
    for (u32 i = 0; i < sizeof(header->reserved1); i++) if (header->reserved1[i] != 0xFF) return 1;
    for (u32 i = 0; i < sizeof(header->reserved2); i++) if (header->reserved2[i] != 0xFF) return 1;
    for (u32 i = 0; i < sizeof(header->reserved3); i++) if (header->reserved3[i] != 0xFF) return 1;
    
    // all fine if arriving here
    return 0;
}

// http://problemkaputt.de/gbatek.htm#gbacartridgeheader
u32 ValidateAgbHeader(AgbHeader* agb) {
    const u8 logo_sha[0x20] = { AGBLOGO_SHA256 };
    u8 logo[0x9C];
    
    // check fixed value
    if (agb->fixed != 0x96) return 1;
    
    // header checksum
    u8* hdr = (u8*) agb;
    u8 checksum = 0x00 - 0x19;
    for (u32 i = 0xA0; i < 0xBD; i++)
        checksum -= hdr[i];
    if (agb->checksum != checksum) return 1;
    
    // logo SHA check
    memcpy(logo, agb->logo, 0x9C);
    logo[0x98] &= ~0x84; 
    logo[0x9A] &= ~0x03;
    if (sha_cmp(logo_sha, logo, 0x9C, SHA256_MODE) != 0)
        return 1;
    
    return 0;
}

/* u32 ValidateAgbVc(void* data, u32 len) {
    const u8 magic[] = { GBAVC_MAGIC };
    
    if (len < sizeof(AgbHeader) + sizeof(AgbVcFooter))
        return 1;
    
    AgbHeader* header = (AgbHeader*) data;
    AgbVcFooter* footer = (AgbVcFooter*) (((u8*) data) + len - sizeof(AgbVcFooter));
    
    if ((ValidateAgbHeader(header) != 0) || (memcmp(footer->magic, magic, sizeof(magic)) != 0) ||
        (footer->rom_size != len - sizeof(AgbVcFooter)))
        return 1;
        
    return 0;
} */

// basically reverse ValidateAgbSaveHeader()
/* u32 BuildAgbSaveHeader(AgbSaveHeader* header, u64 title_id, u32 save_size) {
    const u8 magic[] = { AGBSAVE_MAGIC };
    
    memset(header, 0x00, sizeof(AgbSaveHeader));
    memset(header->reserved0, 0xFF, sizeof(header->reserved0));
    memset(header->reserved1, 0xFF, sizeof(header->reserved1));
    memset(header->reserved2, 0xFF, sizeof(header->reserved2));
    memset(header->reserved3, 0xFF, sizeof(header->reserved3));
    
    memcpy(header->magic, magic, sizeof(magic));
    header->unknown0 = 0x01;
    header->title_id = title_id;
    header->save_start = 0x200;
    header->save_size = save_size;
    
    sdmmc_get_cid(0, (u32*) (void*) &(header->sd_cid));
    
    return 0;
} */
