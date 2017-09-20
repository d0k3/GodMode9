#include "agbsave.h"
#include "sha.h"
#include "aes.h"


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
