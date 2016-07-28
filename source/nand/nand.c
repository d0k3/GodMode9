#include "fs.h"
#include "platform.h"
#include "aes.h"
#include "sha.h"
#include "sdmmc.h"
#include "nand.h"
#include "image.h"

#define NAND_BUFFER ((u8*)0x21300000)
#define NAND_BUFFER_SIZE (0x100000) // must be multiple of 0x200
#define NAND_MIN_SECTORS ((GetUnitPlatform() == PLATFORM_N3DS) ? 0x26C000 : 0x1D7800)

static u8 slot0x05KeyY[0x10] = { 0x00 }; // need to load this from FIRM0 / external file
static u8 slot0x05KeyY_sha256[0x20] = { // hash for slot0x05KeyY (16 byte)
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
static u8 OtpSha256[32] = { 0 };

static u32 emunand_base_sector = 0x000000;

bool LoadKeyFromFile(const char* folder, u8* keydata, u32 keyslot, char type, char* id)
{
    char path[256]; // should be enough
    u8 key_magic[16];
    u8 buffer[32];
    u8* key = buffer + 16;
    bool found = false;
    
    // check the obvious
    if (keyslot >= 0x40)
        return false; // invalid keyslot
    if ((type != 'X') && (type != 'Y') && (type != 'N'))
        return false; // invalid keytype 
    
    // search 'aeskeydb.bin' file - setup
    snprintf(path, 256, "%s/aeskeydb.bin", folder);
    memset(key_magic, 0x00, 16);
    key_magic[0] = keyslot;
    key_magic[1] = type;
    if (id) strncpy((char*) key_magic + 2, id, 10);
    
    // try to find key in 'aeskeydb.bin' file
    for (u32 p = 0; FileGetData(path, buffer, 32, p) == 32; p += 32) {
        if (memcmp(buffer, key_magic, 12) == 0) {
            found = true;
            break;
        }
    }
    if (found && buffer[15]) { // encrypted key -> decrypt first
        u8 ctr[16] __attribute__((aligned(32)));
        u8 keyY[16] __attribute__((aligned(32)));
        memset(ctr, 0x00, 16);
        memset(keyY, 0x00, 16);
        memcpy(ctr, key_magic, 12);
        setup_aeskeyY(0x2C, keyY);
        use_aeskey(0x2C);
        set_ctr(ctr);
        aes_decrypt((void*) key, (void*) key, 1, AES_CNT_CTRNAND_MODE);
    }
    
    // try legacy slot0x??Key?.bin file
    if (!found) {
        snprintf(path, 256, "%s/slot0x%02XKey%.10s.bin", folder, (unsigned int) keyslot,
            (id) ? id : (type == 'X') ? "X" : (type == 'Y') ? "Y" : "");
        if (FileGetData(path, key, 16, 0) == 16)
            found = true;
    }
    
    // out of options here
    if (!found) return false;
    
    // now, setup the key
    if (type == 'X') { // keyX
        setup_aeskeyX(keyslot, key);
    } else if (type == 'Y') { // keyY
        setup_aeskeyY(keyslot, key);
    } else { // normalKey
        setup_aeskey(keyslot, key);
    }
    use_aeskey(keyslot);
    
    // return the key if memory provided
    if (keydata) memcpy(keydata, key, 16);
    
    return true;
}

bool InitNandCrypto(void)
{   
    // part #0: KeyX / KeyY for secret sector 0x96
    // on a9lh this MUST be run before accessing the SHA register in any other way
    if ((*(u32*) 0x101401C0) == 0) { // for a9lh
        // store the current SHA256 from register
        memcpy(OtpSha256, (void*)REG_SHAHASH, 32);
    } else {
        u8 otp[0x100];
        if ((FileGetData("0:/otp.bin", otp, 0x100, 0) == 0x100) ||
            (FileGetData("0:/otp0x108.bin", otp, 0x100, 0) == 0x100) ||
            (FileGetData("0:/Decrypt9/otp.bin", otp, 0x100, 0) == 0x100) ||
            (FileGetData("0:/Decrypt9/otp0x108.bin", otp, 0x100, 0) == 0x100) ||
            (FileGetData("0:/files9/otp.bin", otp, 0x100, 0) == 0x100) ||
            (FileGetData("0:/files9/otp0x108.bin", otp, 0x100, 0) == 0x100))
            sha_quick(OtpSha256, otp, 0x90, SHA256_MODE);
    }
        
    // part #1: Get NAND CID, set up TWL/CTR counter
    u32 NandCid[4];
    u8 shasum[32];
    
    sdmmc_get_cid( 1, NandCid);
    sha_quick(shasum, (u8*) NandCid, 16, SHA256_MODE);
    memcpy(CtrNandCtr, shasum, 16);
    sha_quick(shasum, (u8*) NandCid, 16, SHA1_MODE);
    for(u32 i = 0; i < 16; i++) // little endian and reversed order
        TwlNandCtr[i] = shasum[15-i];
    
    // part #2: TWL KEY
    // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
    if ((*(vu32*) 0x101401C0) == 0) { // only for a9lh
        u32* TwlCustId = (u32*) (0x01FFB808);
        u8 TwlKeyX[16];
        u8 TwlKeyY[16];
        
        // thanks b1l1s & Normmatt
        // see source from https://gbatemp.net/threads/release-twltool-dsi-downgrading-save-injection-etc-multitool.393488/
        const char* nintendo = "NINTENDO";
        u32 TwlKeyXW0 = (TwlCustId[0] ^ 0xB358A6AF) | 0x80000000;
        u32 TwlKeyXW3 = TwlCustId[1] ^ 0x08C267B7;
        memcpy(TwlKeyX +  4, nintendo, 8);
        memcpy(TwlKeyX +  0, &TwlKeyXW0, 4);
        memcpy(TwlKeyX + 12, &TwlKeyXW3, 4);
        
        // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
        u32 TwlKeyYW3 = 0xE1A00005;
        memcpy(TwlKeyY, (u8*) 0x01FFD3C8, 12);
        memcpy(TwlKeyY + 12, &TwlKeyYW3, 4);
        
        setup_aeskeyX(0x03, TwlKeyX);
        setup_aeskeyY(0x03, TwlKeyY);
        use_aeskey(0x03);
    }
    
    // part #3: CTRNAND N3DS KEY
    // thanks AuroraWright and Gelex for advice on this
    // see: https://github.com/AuroraWright/Luma3DS/blob/master/source/crypto.c#L347
    if ((*(vu32*) 0x101401C0) == 0) { // only for a9lh
        u8 ctr[16] __attribute__((aligned(32)));
        u8 keyY[16] __attribute__((aligned(32)));
        u8 header[0x200];
        
        // section 2 header of FIRM0
        // this is @0x066A00 in FIRM90 & FIRM81
        static u32 offsetSection2 = 0x066A00;
        ReadNandSectors(header, 0x58980 + (offsetSection2 / 0x200), 1, 0x06, NAND_SYSNAND);
        memcpy(keyY, header + 0x10, 0x10); // 0x15 keyY
        
        // try FRIM90 & FIRM81 offsets, search for the key
        for (u32 fver = 0; fver < 2; fver++) {
            static u32 offset0x05KeyY[2] = { 0x0EB014, 0x0EB24C };
            u32 offset = offset0x05KeyY[fver];
            u8 sector[0x200];
        
            // sector containing the slot0x05 keyY
            // key is encrypted @0x0EB014 in the FIRM90
            // key is encrypted @0x0EB24C in the FIRM81
            ReadNandSectors(sector, 0x58980 + (offset / 0x200), 1, 0x06, NAND_SYSNAND);
            
            // decrypt the sector, get the key
            memcpy(ctr, header + 0x20, 0x10); // 0x15 counter
            add_ctr(ctr, (offset - (offset % 0x200) - (offsetSection2 + 0x800)) / 16);
            for (u32 i = 0x0; i < 0x200; i += 0x10) {
                setup_aeskeyY(0x15, keyY);
                use_aeskey(0x15);
                set_ctr(ctr);
                aes_decrypt(sector + i, sector + i, 1, AES_CNT_CTRNAND_MODE);
                add_ctr(ctr, 0x1);
            }
            memcpy(slot0x05KeyY, sector + (offset % 0x200), 16);
            
            // check the key
            sha_quick(shasum, slot0x05KeyY, 16, SHA256_MODE);
            if (memcmp(shasum, slot0x05KeyY_sha256, 32) == 0) {
                setup_aeskeyY(0x05, slot0x05KeyY);
                use_aeskey(0x05);
                break;
            }
        }
        
        if ((memcmp(shasum, slot0x05KeyY_sha256, 32) != 0) && // last resort
            (!LoadKeyFromFile("0:", slot0x05KeyY, 0x05, 'Y', NULL)) &&
            (!LoadKeyFromFile("0:/Decrypt9", slot0x05KeyY, 0x05, 'Y', NULL)) &&
            (!LoadKeyFromFile("0:/files9", slot0x05KeyY, 0x05, 'Y', NULL))) {};
    }
    
    return true;
}

bool CheckSlot0x05Crypto(void)
{
    // step #1 - check the slot0x05KeyY SHA-256
    u8 shasum[32];
    sha_quick(shasum, slot0x05KeyY, 16, SHA256_MODE);
    if (memcmp(shasum, slot0x05KeyY_sha256, 32) == 0)
        return true;
    
    // step #2 - check actual CTRNAND magic
    const u8 magic[8] = {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}; 
    const u32 sector = 0x05CAD7;
    u8 buffer[0x200];
    ReadNandSectors(buffer, sector, 1, 0x05, NAND_SYSNAND);
    if (memcmp(buffer, magic, 8) == 0)
        return true;
    
    // failed if we arrive here
    return false;
}

bool CheckSector0x96Crypto(void)
{
    const u8 zeroes[32] = { 0 };
    return !(memcmp(OtpSha256, zeroes, 32) == 0);
}

void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot)
{
    u32 mode = (sector >= (0x0B100000 / 0x200)) ? AES_CNT_CTRNAND_MODE : AES_CNT_TWLNAND_MODE;
    u8 ctr[16] __attribute__((aligned(32)));
    u32 blocks = count * (0x200 / 0x10);
    
    // copy NAND CTR and increment it
    memcpy(ctr, (sector >= (0x0B100000 / 0x200)) ? CtrNandCtr : TwlNandCtr, 16);
    add_ctr(ctr, sector * (0x200/0x10));
    
    // decrypt the data
    use_aeskey(keyslot);
    ctr_decrypt((void*) buffer, (void*) buffer, blocks, mode, ctr);
}

void CryptSector0x96(u8* buffer, bool encrypt)
{
    u32 mode = encrypt ? AES_CNT_ECB_ENCRYPT_MODE : AES_CNT_ECB_DECRYPT_MODE;
    
    // setup the key
    setup_aeskeyX(0x11, OtpSha256);
    setup_aeskeyY(0x11, OtpSha256 + 16);
    
    // decrypt the sector
    use_aeskey(0x11);
    for (u32 b = 0x0; b < 0x200; b += 0x10, buffer += 0x10)
        aes_decrypt((void*) buffer, (void*) buffer, 1, mode);
}

int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, u32 nand_src)
{
    if (!count) return 0; // <--- just to be safe
    if (nand_src == NAND_EMUNAND) { // EmuNAND
        int errorcode = 0;
        if ((sector == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
            errorcode = sdmmc_sdcard_readsectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, buffer);
            sector = 1;
            count--;
            buffer += 0x200;
        }
        errorcode = (!errorcode && count) ? sdmmc_sdcard_readsectors(emunand_base_sector + sector, count, buffer) : errorcode;
        if (errorcode) return errorcode;
    } else if (nand_src == NAND_IMGNAND) { // ImgNAND
        int errorcode = ReadImageSectors(buffer, sector, count);
        if (errorcode) return errorcode;
    } else if (nand_src == NAND_SYSNAND) { // SysNAND
        int errorcode = sdmmc_nand_readsectors(sector, count, buffer);
        if (errorcode) return errorcode;   
    } else {
        return -1;
    }
    if ((keyslot == 0x11) && (sector == 0x96)) CryptSector0x96(buffer, false);
    else if (keyslot < 0x40) CryptNand(buffer, sector, count, keyslot);
    
    return 0;
}

int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 nand_dst)
{
    // buffer must not be changed, so this is a little complicated
    for (u32 s = 0; s < count; s += (NAND_BUFFER_SIZE / 0x200)) {
        u32 pcount = min((NAND_BUFFER_SIZE/0x200), (count - s));
        memcpy(NAND_BUFFER, buffer + (s*0x200), pcount * 0x200);
        if ((keyslot == 0x11) && (sector == 0x96)) CryptSector0x96(NAND_BUFFER, true);
        else if (keyslot < 0x40) CryptNand(NAND_BUFFER, sector + s, pcount, keyslot);
        if (nand_dst == NAND_EMUNAND) {
            int errorcode = 0;
            if ((sector + s == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
                errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, NAND_BUFFER);
                errorcode = (!errorcode && (pcount > 1)) ? sdmmc_sdcard_writesectors(emunand_base_sector + 1, pcount - 1, NAND_BUFFER + 0x200) : errorcode;
            } else errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + sector + s, pcount, NAND_BUFFER);
            if (errorcode) return errorcode;
        } else if (nand_dst == NAND_IMGNAND) {
            int errorcode = WriteImageSectors(NAND_BUFFER, sector + s, pcount);
            if (errorcode) return errorcode;
        } else if (nand_dst == NAND_SYSNAND) {
            int errorcode = sdmmc_nand_writesectors(sector + s, pcount, NAND_BUFFER);
            if (errorcode) return errorcode;
        } else {
            return -1;
        }
    }
    
    return 0;
}

u32 CheckNandType(u32 nand_src)
{
    if (ReadNandSectors(NAND_BUFFER, 0, 1, 0xFF, nand_src) != 0)
        return 0;
    if (memcmp(NAND_BUFFER + 0x100, nand_magic_n3ds, 0x60) == 0) {
        return (GetUnitPlatform() == PLATFORM_3DS) ? 0 : NAND_TYPE_N3DS;
    } else if (memcmp(NAND_BUFFER + 0x100, nand_magic_o3ds, 0x60) == 0) {
        return (GetUnitPlatform() == PLATFORM_3DS) ? NAND_TYPE_O3DS : NAND_TYPE_NO3DS;
    }
    
    return 0;
}

u64 GetNandSizeSectors(u32 nand_src)
{
    u32 sysnand_sectors = getMMCDevice(0)->total_size;
    if (nand_src == NAND_EMUNAND) { // for EmuNAND
        u32 emunand_max_sectors = GetPartitionOffsetSector("0:") - (emunand_base_sector + 1); // +1 for safety
        u32 emunand_min_sectors = (emunand_base_sector % 0x200000 == 0) ? sysnand_sectors : NAND_MIN_SECTORS;
        if (emunand_max_sectors >= sysnand_sectors) return sysnand_sectors;
        else return (emunand_min_sectors > emunand_max_sectors) ? 0 : emunand_min_sectors;
    } else if (nand_src == NAND_IMGNAND) {
        u32 img_sectors = (GetMountState() == IMG_NAND) ? GetMountSize() / 0x200 : 0;
        return (img_sectors >= sysnand_sectors) ? sysnand_sectors : (img_sectors >= NAND_MIN_SECTORS) ? NAND_MIN_SECTORS : 0;
    } else return sysnand_sectors; // for SysNAND
}

bool InitEmuNandBase(void)
{
    emunand_base_sector = 0x000000; // GW type EmuNAND
    if (CheckNandType(NAND_EMUNAND))
        return true;
    
    emunand_base_sector = 0x000001; // RedNAND type EmuNAND
    if (CheckNandType(NAND_EMUNAND))
        return true;
    
    return false;
}
