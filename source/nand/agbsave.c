#include "agbsave.h"
#include "sha.h"
#include "aes.h"


u32 ValidateAgbSaveHeader(AgbSaveHeader* header) {
    u8 magic[] = { AGBSAVE_MAGIC };
    
    // basic checks
    if ((memcmp(header->magic, magic, sizeof(magic)) != 0) ||
        (header->unknown0 != 1) || (header->save_start != 0x200) ||
        (header->save_size > AGBSAVE_MAX_SSIZE))
        return 1;
        
    // reserved area checks
    for (u32 i = 0; i < sizeof(header->reserved0); i++) if (header->reserved0[i] != 0xFF) return 1;
    for (u32 i = 0; i < sizeof(header->reserved1); i++) if (header->reserved1[i] != 0xFF) return 1;
    for (u32 i = 0; i < sizeof(header->reserved2); i++) if (header->reserved2[i] != 0xFF) return 1;
    for (u32 i = 0; i < sizeof(header->reserved3); i++) if (header->reserved3[i] != 0xFF) return 1;
    
    // all fine if arriving here
    return 0;
}

u32 LoadAgbSave(u32 nand_src, u8* agbsave, u32 max_size, NandPartitionInfo* info, bool header_only) {
    AgbSaveHeader* header = (AgbSaveHeader*) agbsave;
    
    // need at least room for the header
    if (max_size < sizeof(AgbSaveHeader)) return 1;
    
    // load the header
    if ((GetNandPartitionInfo(info, NP_TYPE_AGB, NP_SUBTYPE_CTR, 0, nand_src) != 0) ||
        (ReadNandSectors(agbsave, info->sector, 1, info->keyslot, nand_src) != 0) ||
        (ValidateAgbSaveHeader(header) != 0) ||
        (sizeof(AgbSaveHeader) + header->save_size > info->count * 0x200))
        return 1;
        
    // done if we only want the header
    if (header_only) return 0;
        
    // load the savegame
    if ((sizeof(AgbSaveHeader) + header->save_size > max_size) ||
        (ReadNandBytes(agbsave + 0x200, (info->sector+1) * 0x200, header->save_size, info->keyslot, nand_src) != 0))
        return 1;
        
    return 0;
}

u32 GetAgbSaveSize(u32 nand_src) {
    AgbSaveHeader* header = (AgbSaveHeader*) NAND_BUFFER;
    NandPartitionInfo info;
    if (LoadAgbSave(nand_src, NAND_BUFFER, NAND_BUFFER_SIZE, &info, true) != 0)
        return 1;
    return header->save_size; // it's recommended to also check the CMAC
}

u32 CheckAgbSaveCmac(u32 nand_src) {
    u8* agbsave = (u8*) NAND_BUFFER;
    AgbSaveHeader* header = (AgbSaveHeader*) agbsave;
    NandPartitionInfo info;
    if (LoadAgbSave(nand_src, agbsave, NAND_BUFFER_SIZE, &info, false) != 0)
        return 1;
    
    u8 cmac[16] __attribute__((aligned(32)));
    u8 shasum[32];
    sha_quick(shasum, agbsave + 0x30, (0x200 - 0x30) + header->save_size, SHA256_MODE);
    use_aeskey(0x24);
    aes_cmac(shasum, cmac, 2);
    
    return (memcmp(cmac, header->cmac, 16) == 0) ? 0 : 1;
}

u32 FixAgbSaveCmac(u32 nand_dst) {
    u8* agbsave = (u8*) NAND_BUFFER;
    AgbSaveHeader* header = (AgbSaveHeader*) agbsave;
    NandPartitionInfo info;
    if (LoadAgbSave(nand_dst, agbsave, NAND_BUFFER_SIZE, &info, false) != 0)
        return 1;
        
    u8 cmac[16] __attribute__((aligned(32)));
    u8 shasum[32];
    sha_quick(shasum, agbsave + 0x30, (0x200 - 0x30) + header->save_size, SHA256_MODE);
    use_aeskey(0x24);
    aes_cmac(shasum, cmac, 2);
    memcpy(header->cmac, cmac, 16);
    
    // set CFG_BOOTENV to 0x7 so the save is taken over
    // https://www.3dbrew.org/wiki/CONFIG_Registers#CFG_BOOTENV
    *(u32*) 0x10010000 = 0x7;
    
    return (WriteNandSectors(header, info.sector, 1, info.keyslot, nand_dst) == 0) ? 0 : 1;
}
