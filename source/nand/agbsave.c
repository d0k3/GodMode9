#include "agbsave.h"
#include "sha.h"
#include "aes.h"

u32 GetAgbSaveSize(u32 nand_src) {
    AgbSave* agbsave = (AgbSave*) NAND_BUFFER;
    if (ReadNandSectors((u8*) agbsave, SECTOR_AGBSAVE, 1, 0x07, nand_src) != 0)
        return 0;
    return agbsave->save_size; // it's recommended to also check the CMAC
}

u32 CheckAgbSaveCmac(u32 nand_src) {
    u8 magic[] = { AGBSAVE_MAGIC };
    
    AgbSave* agbsave = (AgbSave*) NAND_BUFFER;
    if ((ReadNandSectors((u8*) agbsave, SECTOR_AGBSAVE, 1, 0x07, nand_src) != 0) ||
        (memcmp(agbsave->magic, magic, sizeof(magic)) != 0) ||
        (ReadNandBytes(agbsave->savegame, (SECTOR_AGBSAVE+1) * 0x200, agbsave->save_size, 0x07, nand_src) != 0))
        return 1;
    
    u8 cmac[16] __attribute__((aligned(32)));
    u8 shasum[32];
    sha_quick(shasum, ((u8*) agbsave) + 0x30, (0x200 - 0x30) + agbsave->save_size, SHA256_MODE);
    use_aeskey(0x24);
    aes_cmac(shasum, cmac, 2);
    
    return (memcmp(cmac, agbsave->cmac, 16) == 0) ? 0 : 1;
}

u32 FixAgbSaveCmac(u32 nand_dst) {
    AgbSave* agbsave = (AgbSave*) NAND_BUFFER;
    if ((ReadNandSectors((u8*) agbsave, SECTOR_AGBSAVE, 1, 0x07, nand_dst) != 0) ||
        (ReadNandBytes(agbsave->savegame, (SECTOR_AGBSAVE+1) * 0x200, agbsave->save_size, 0x07, nand_dst) != 0))
        return 1;
        
    u8 cmac[16] __attribute__((aligned(32)));
    u8 shasum[32];
    sha_quick(shasum, ((u8*) agbsave) + 0x30, (0x200 - 0x30) + agbsave->save_size, SHA256_MODE);
    use_aeskey(0x24);
    aes_cmac(shasum, cmac, 2);
    memcpy(agbsave->cmac, cmac, 16);
    
    // set CFG_BOOTENV = 0x7 so the save is taken over
    // https://www.3dbrew.org/wiki/CONFIG_Registers#CFG_BOOTENV
    *(u32*) 0x10010000 = 0x7;
    
    return (WriteNandSectors((u8*) agbsave, SECTOR_AGBSAVE, 1, 0x07, nand_dst) == 0) ? 0 : 1;
}
