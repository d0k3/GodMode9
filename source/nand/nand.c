#include "fs.h"
#include "platform.h"
#include "aes.h"
#include "sha.h"
#include "sdmmc.h"
#include "nand.h"

#define NAND_BUFFER ((u8*)0x21100000)
#define NAND_BUFFER_SIZE (0x100000) // must be multiple of 0x200

static u8 slot0x05KeyY[0x10] = { 0x00 }; // need to load this from file
static u8 slot0x05KeyY_sha256[0x20] = { // hash for slot0x05KeyY file
    0x98, 0x24, 0x27, 0x14, 0x22, 0xB0, 0x6B, 0xF2, 0x10, 0x96, 0x9C, 0x36, 0x42, 0x53, 0x7C, 0x86,
    0x62, 0x22, 0x5C, 0xFD, 0x6F, 0xAE, 0x9B, 0x0A, 0x85, 0xA5, 0xCE, 0x21, 0xAA, 0xB6, 0xC8, 0x4D
};

static u8 nand_magic_n3ds[0x60] = { // NCSD NAND header N3DS magic
    0x4E, 0x43, 0x53, 0x44, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x80, 0x89, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0x80, 0xA9, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x80, 0xC9, 0x05, 0x00, 0x80, 0xF6, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 nand_magic_o3ds[0x60] = { // NCSD NAND header O3DS magic
    0x4E, 0x43, 0x53, 0x44, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x80, 0x89, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0x80, 0xA9, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x80, 0xC9, 0x05, 0x00, 0x80, 0xAE, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 CtrNandCtr[16];
static u8 TwlNandCtr[16];

static u32 emunand_base_sector = 0x000000;


bool InitNandCrypto(void)
{
    // STEP #1: Get NAND CID, set up TWL/CTR counter
    u8 NandCid[16];
    u8 shasum[32];
    
    sdmmc_get_cid( 1, (uint32_t*) NandCid);
    sha_init(SHA256_MODE);
    sha_update(NandCid, 16);
    sha_get(shasum);
    memcpy(CtrNandCtr, shasum, 16);
    sha_init(SHA1_MODE);
    sha_update(NandCid, 16);
    sha_get(shasum);
    for(u32 i = 0; i < 16; i++) // little endian and reversed order
        TwlNandCtr[i] = shasum[15-i];
    
    // part #2: TWL KEY
    // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
    u32* TwlCustId = (u32*) (0x01FFB808);
    u8 TwlKeyX[16];
    u8 TwlKeyY[16];
    
    // thanks b1l1s & Normmatt
    // see source from https://gbatemp.net/threads/release-twltool-dsi-downgrading-save-injection-etc-multitool.393488/
    const char* nintendo = "NINTENDO";
    u32* TwlKeyXW = (u32*) TwlKeyX;
    TwlKeyXW[0] = (TwlCustId[0] ^ 0xB358A6AF) | 0x80000000;
    TwlKeyXW[3] = TwlCustId[1] ^ 0x08C267B7;
    memcpy(TwlKeyX + 4, nintendo, 8);
    
    // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
    u32 TwlKeyYW3 = 0xE1A00005;
    memcpy(TwlKeyY, (u8*) 0x01FFD3C8, 12);
    memcpy(TwlKeyY + 12, &TwlKeyYW3, 4);
    
    setup_aeskeyX(0x03, TwlKeyX);
    setup_aeskeyY(0x03, TwlKeyY);
    use_aeskey(0x03);
    
    // part #3: CTRNAND N3DS KEY
    if (FileGetData("0:/slot0x05KeyY.bin", slot0x05KeyY, 16, 0)) {
        setup_aeskeyY(0x05, slot0x05KeyY);
        use_aeskey(0x05);
    }
    
    
    return true;
}

bool CheckSlot0x05Crypto(void)
{
    // step #1 - check the slot0x05KeyY SHA-256
    u8 shasum[32];
    sha_init(SHA256_MODE);
    sha_update(slot0x05KeyY, 16);
    sha_get(shasum);
    if (memcmp(shasum, slot0x05KeyY_sha256, 32) == 0)
        return true;
    
    // step #2 - check actual CTRNAND magic
    const u8 magic[8] = {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}; 
    const u32 sector = 0x05CAD7;
    u8 buffer[0x200];
    for (u32 nand = 0; nand < 2; nand++) {
        ReadNandSectors(buffer, sector, 1, 0x05, nand);
        if (memcmp(buffer, magic, 8) == 0)
            return true;
    }
    
    // failed if we arrive here
    return false;
}

void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot)
{
    u32 mode = (sector >= (0x0B100000 / 0x200)) ? AES_CNT_CTRNAND_MODE : AES_CNT_TWLNAND_MODE;
    u8 ctr[16] __attribute__((aligned(32)));
    
    // copy NAND CTR and increment it
    memcpy(ctr, (sector >= (0x0B100000 / 0x200)) ? CtrNandCtr : TwlNandCtr, 16);
    add_ctr(ctr, sector * (0x200/0x10));
    
    // decrypt the data
    use_aeskey(keyslot);
    for (u32 s = 0; s < count; s++) {
        for (u32 b = 0x0; b < 0x200; b += 0x10, buffer += 0x10) {
            set_ctr(ctr);
            aes_decrypt((void*) buffer, (void*) buffer, 1, mode);
            add_ctr(ctr, 0x1);
        }
    }
}

int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, bool read_emunand)
{
    if (read_emunand) {
        int errorcode = 0;
        if ((sector == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
            errorcode = sdmmc_sdcard_readsectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, buffer);
            sector = 1;
            count--;
            buffer += 0x200;
        }
        errorcode = (!errorcode && count) ? sdmmc_sdcard_readsectors(emunand_base_sector + sector, count, buffer) : errorcode;
        if (errorcode) return errorcode;
    } else {
        int errorcode = sdmmc_nand_readsectors(sector, count, buffer);
        if (errorcode) return errorcode;   
    }
    if (keyslot < 0x40) CryptNand(buffer, sector, count, keyslot);
    
    return 0;
}

int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, bool write_emunand)
{
    // buffer must not be changed, so this is a little complicated
    for (u32 s = 0; s < count; s += (NAND_BUFFER_SIZE / 0x200)) {
        u32 pcount = min((NAND_BUFFER_SIZE/0x200), (count - s));
        memcpy(NAND_BUFFER, buffer + (s*0x200), pcount * 0x200);
        if (keyslot < 0x40) CryptNand(NAND_BUFFER, sector + s, pcount, keyslot);
        if (write_emunand) {
            int errorcode = 0;
            if ((sector + s == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
                errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, NAND_BUFFER);
                errorcode = (!errorcode && (pcount > 1)) ? sdmmc_sdcard_writesectors(emunand_base_sector + 1, pcount - 1, NAND_BUFFER + 0x200) : errorcode;
            } else errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + sector + s, pcount, NAND_BUFFER);
            if (errorcode) return errorcode;
        } else {
            int errorcode = sdmmc_nand_writesectors(sector + s, pcount, NAND_BUFFER);
            if (errorcode) return errorcode;   
        }
    }
    
    return 0;
}

u8 CheckNandType(bool check_emunand)
{
    if (ReadNandSectors(NAND_BUFFER, 0, 1, 0xFF, check_emunand) != 0)
        return NAND_TYPE_UNK;
    if (memcmp(NAND_BUFFER + 0x100, nand_magic_n3ds, 0x60) == 0) {
        return NAND_TYPE_N3DS;
    } else if (memcmp(NAND_BUFFER + 0x100, nand_magic_o3ds, 0x60) == 0) {
        return (GetUnitPlatform() == PLATFORM_3DS) ? NAND_TYPE_O3DS : NAND_TYPE_NO3DS;
    }
    
    return NAND_TYPE_UNK;
}

u64 GetNandSizeSectors(bool size_emunand)
{
    if (size_emunand) { // for EmuNAND
        u32 emunand_max_sectors = GetPartitionOffsetSector("0:") - (emunand_base_sector + 1); // +1 for safety
        u32 emunand_min_sectors = (emunand_base_sector % 0x200000 == 0) ? getMMCDevice(0)->total_size :
            (GetUnitPlatform() == PLATFORM_N3DS) ? 0x26C000 : 0x1D7800;
        return (emunand_min_sectors > emunand_max_sectors) ? 0 : emunand_min_sectors;
    } else return getMMCDevice(0)->total_size; // for SysNAND
}

bool InitEmuNandBase(void)
{
    emunand_base_sector = 0x000000; // GW type EmuNAND
    if (CheckNandType(true) != NAND_TYPE_UNK)
        return true;
    
    emunand_base_sector = 0x000001; // RedNAND type EmuNAND
    if (CheckNandType(true) != NAND_TYPE_UNK)
        return true;
    
    if (GetPartitionOffsetSector("0:") > getMMCDevice(0)->total_size)
        emunand_base_sector = 0x000000; // keep unknown EmuNAND as RedNAND only if space is low
    return false;
}
