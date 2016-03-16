#include "fs.h"
#include "draw.h"
#include "hid.h"
#include "platform.h"
#include "aes.h"
#include "sha.h"
#include "sdmmc.h"
#include "nand.h"

#define NAND_BUFFER ((u8*)0x21100000)
#define NAND_BUFFER_SIZE (0x100000) // must be multiple of 0x200

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
    if (GetUnitPlatform() == PLATFORM_N3DS) {
        u8 CtrNandKeyY[16];
        
        if (FileGetData("0:/slot0x05KeyY.bin", CtrNandKeyY, 16, 0)) {
            setup_aeskeyY(0x05, CtrNandKeyY);
            use_aeskey(0x05);
        }
    }
    
    
    return true;
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

bool InitEmuNandBase(void)
{
    if (GetPartitionOffsetSector("0:") <= getMMCDevice(0)->total_size)
        return false;
    
    emunand_base_sector = 0x000000; // GW type EmuNAND
    if (CheckNandType(true) != NAND_TYPE_UNK)
        return true;
    
    emunand_base_sector = 0x000001; // RedNAND type EmuNAND
    if (CheckNandType(true) != NAND_TYPE_UNK)
        return true;
    
    emunand_base_sector = 0x000000;
    return false;
}

u32 GetEmuNandBase(void)
{
    return emunand_base_sector;
}

u32 SwitchEmuNandBase(int start_sector)
{
    // switching code goes here
    return emunand_base_sector;
}


