#include "nand.h"
#include "fsdrive.h"
#include "fsutil.h"
#include "unittype.h"
#include "keydb.h"
#include "aes.h"
#include "sha.h"
#include "fatmbr.h"
#include "sdmmc.h"
#include "image.h"

#define NAND_MIN_SECTORS ((!IS_O3DS) ? NAND_MIN_SECTORS_N3DS : NAND_MIN_SECTORS_O3DS)

#define KEY95_SHA256    ((IS_DEVKIT) ? slot0x11Key95dev_sha256 : slot0x11Key95_sha256)
#define SECTOR_SHA256   ((IS_DEVKIT) ? sector0x96dev_sha256 : sector0x96_sha256)

// see: https://www.3dbrew.org/wiki/NCSD#NCSD_header
static const u32 np_keyslots[9][4] = { // [NP_TYPE][NP_SUBTYPE]
    { 0xFF, 0xFF, 0xFF, 0xFF }, // none
    { 0xFF, 0x03, 0x04, 0x05 }, // standard
    { 0xFF, 0x03, 0x04, 0x05 }, // FAT (custom, not in NCSD)
    { 0xFF, 0xFF, 0x06, 0xFF }, // FIRM
    { 0xFF, 0xFF, 0x07, 0xFF }, // AGBSAVE
    { 0xFF, 0xFF, 0xFF, 0xFF }, // NCSD (custom)
    { 0xFF, 0xFF, 0xFF, 0xFF }, // D0K3 (custom)
    { 0xFF, 0xFF, 0xFF, 0x11 }, // SECRET (custom)
    { 0xFF, 0xFF, 0xFF, 0xFF }  // BONUS (custom)
};

static u8 slot0x05KeyY[0x10] = { 0x00 }; // need to load this from FIRM0 / external file
static const u8 slot0x05KeyY_sha256[0x20] = { // hash for slot0x05KeyY (16 byte)
    0x98, 0x24, 0x27, 0x14, 0x22, 0xB0, 0x6B, 0xF2, 0x10, 0x96, 0x9C, 0x36, 0x42, 0x53, 0x7C, 0x86,
    0x62, 0x22, 0x5C, 0xFD, 0x6F, 0xAE, 0x9B, 0x0A, 0x85, 0xA5, 0xCE, 0x21, 0xAA, 0xB6, 0xC8, 0x4D
};
static const u8 slot0x24KeyY_sha256[0x20] = { // hash for slot0x24KeyY (16 byte) 
    0x5F, 0x04, 0x01, 0x22, 0x95, 0xB2, 0x23, 0x70, 0x12, 0x40, 0x53, 0x30, 0xC0, 0xA7, 0xBF, 0x7C, 
    0xD4, 0x40, 0x92, 0x25, 0xD1, 0x9D, 0xA2, 0xDE, 0xCD, 0xC7, 0x12, 0x97, 0x08, 0x46, 0x54, 0xB7
};

static const u8 slot0x11Key95_sha256[0x20] = { // slot0x11Key95 hash (first 16 byte of sector0x96)
    0xBA, 0xC1, 0x40, 0x9C, 0x6E, 0xE4, 0x1F, 0x04, 0xAA, 0xC4, 0xE2, 0x09, 0x5C, 0xE9, 0x4F, 0x78, 
    0x6C, 0x78, 0x5F, 0xAC, 0xEC, 0x7E, 0xC0, 0x11, 0x26, 0x9D, 0x4E, 0x47, 0xB3, 0x64, 0xC4, 0xA5
};

static const u8 slot0x11Key95dev_sha256[0x20] = { // slot0x11Key95 hash (first 16 byte of sector0x96)
    0x97, 0x0E, 0x52, 0x29, 0x63, 0x19, 0x47, 0x51, 0x15, 0xD8, 0x02, 0x7A, 0x22, 0x0F, 0x58, 0x15,
    0xD7, 0x6C, 0xE9, 0xAD, 0xE7, 0xFE, 0x9A, 0x25, 0x4E, 0x4A, 0x0C, 0x82, 0x67, 0xB5, 0x4A, 0x7B
};

// from: https://github.com/AuroraWright/SafeA9LHInstaller/blob/master/source/installer.c#L9-L17
static const u8 sector0x96_sha256[0x20] = { // hash for legit sector 0x96 (different on A9LH)
    0x82, 0xF2, 0x73, 0x0D, 0x2C, 0x2D, 0xA3, 0xF3, 0x01, 0x65, 0xF9, 0x87, 0xFD, 0xCC, 0xAC, 0x5C,
    0xBA, 0xB2, 0x4B, 0x4E, 0x5F, 0x65, 0xC9, 0x81, 0xCD, 0x7B, 0xE6, 0xF4, 0x38, 0xE6, 0xD9, 0xD3
};

// from: https://github.com/SciresM/CTRAesEngine/tree/master/CTRAesEngine/Resources/_byte
static const u8 sector0x96dev_sha256[0x20] = { // hash for legit sector 0x96 (different on A9LH)
    0xB2, 0x91, 0xD9, 0xB1, 0x33, 0x05, 0x79, 0x0D, 0x47, 0xC6, 0x06, 0x98, 0x4C, 0x67, 0xC3, 0x70,
    0x09, 0x54, 0xE3, 0x85, 0xDE, 0x47, 0x55, 0xAF, 0xC6, 0xCB, 0x1D, 0x8D, 0xC7, 0x84, 0x5A, 0x64
};
    
static const u8 nand_magic_n3ds[0x60] = { // NCSD NAND header N3DS magic
    0x4E, 0x43, 0x53, 0x44, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x80, 0x89, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0x80, 0xA9, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x80, 0xC9, 0x05, 0x00, 0x80, 0xF6, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 nand_magic_o3ds[0x60] = { // NCSD NAND header O3DS magic
    0x4E, 0x43, 0x53, 0x44, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x05, 0x00, 0x00, 0x88, 0x05, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x80, 0x89, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0x80, 0xA9, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x80, 0xC9, 0x05, 0x00, 0x80, 0xAE, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u8 twl_mbr[0x42] = { // encrypted version inside the NCSD NAND header (@0x1BE)
    0x00, 0x04, 0x18, 0x00, 0x06, 0x01, 0xA0, 0x3F, 0x97, 0x00, 0x00, 0x00, 0xA9, 0x7D, 0x04, 0x00,
    0x00, 0x04, 0x8E, 0x40, 0x06, 0x01, 0xA0, 0xC3, 0x8D, 0x80, 0x04, 0x00, 0xB3, 0x05, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};

static const u8 ctr_mbr_o3ds[0x42] = { // found at the beginning of the CTRNAND partition (O3DS)
    0x00, 0x05, 0x2B, 0x00, 0x06, 0x02, 0x42, 0x80, 0x65, 0x01, 0x00, 0x00, 0x1B, 0x9F, 0x17, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};

static const u8 ctr_mbr_n3ds[0x42] = { // found at the beginning of the CTRNAND partition (N3DS)
    0x00, 0x05, 0x1D, 0x00, 0x06, 0x02, 0x82, 0x17, 0x57, 0x01, 0x00, 0x00, 0x69, 0xE9, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};

static u8 CtrNandCtr[16];
static u8 TwlNandCtr[16];
static u8 OtpSha256[32] = { 0 };

static u32 emunand_base_sector = 0x000000;


u32 LoadKeyYFromP9(u8* key, const u8* keyhash, u32 offset, u32 keyslot)
{
    static const u32 offsetA9l = 0x066A00; // fixed offset, this only has to work for FIRM90 / FIRM81
    static const u32 sector_firm0 = 0x058980; // standard firm0 sector (this only has to work in A9LH anyways)
    u8 ctr0x15[16] __attribute__((aligned(32)));
    u8 keyY0x15[16] __attribute__((aligned(32)));
    u8 keyY[16] __attribute__((aligned(32)));
    u8 header[0x200];
    
    // check arm9loaderhax
    if (!IS_A9LH || IS_SIGHAX || (offset < (offsetA9l + 0x0800))) return 1;
    
    // section 2 (arm9loader) header of FIRM
    // this is @0x066A00 in FIRM90 & FIRM81
    ReadNandBytes(header, (sector_firm0 * 0x200) + offsetA9l, 0x200, 0x06, NAND_SYSNAND);
    memcpy(keyY0x15, header + 0x10, 0x10); // 0x15 keyY
    memcpy(ctr0x15, header + 0x20, 0x10); // 0x15 counter
    
    // read and decrypt the encrypted keyY
    ReadNandBytes(keyY, (sector_firm0 * 0x200) + offset, 0x10, 0x06, NAND_SYSNAND);
    setup_aeskeyY(0x15, keyY0x15);
    use_aeskey(0x15);
    ctr_decrypt_byte(keyY, keyY, 0x10, offset - (offsetA9l + 0x800), AES_CNT_CTRNAND_MODE, ctr0x15);
    if (key) memcpy(key, keyY, 0x10);
    
    // check the key
    u8 shasum[0x32];
    sha_quick(shasum, keyY, 16, SHA256_MODE);
    if (memcmp(shasum, keyhash, 32) == 0) {
        setup_aeskeyY(keyslot, keyY);
        use_aeskey(keyslot);
        return 0;
    }
    
    return 1;
}

bool InitNandCrypto(void)
{   
    // part #0: KeyX / KeyY for secret sector 0x96
    // on a9lh this MUST be run before accessing the SHA register in any other way
    if (IS_UNLOCKED) { // if OTP is unlocked
        // see: https://www.3dbrew.org/wiki/OTP_Registers
        sha_quick(OtpSha256, (u8*) 0x10012000, 0x90, SHA256_MODE);
    } else if (IS_A9LH) { // for a9lh
        // store the current SHA256 from register
        memcpy(OtpSha256, (void*) REG_SHAHASH, 32);
    } else {
        // load hash via keys?
        const char* base[] = { INPUT_PATHS };
        char path[64];
        u8 otp[0x100];
        for (u32 i = 0; i < 2 * (sizeof(base)/sizeof(char*)); i++) {
            snprintf(path, 64, "%s/%s", base[i/2], (i%2) ? OTP_BIG_NAME : OTP_NAME);
            if (FileGetData(path, otp, 0x100, 0) == 0x100) {
                sha_quick(OtpSha256, otp, 0x90, SHA256_MODE);
                break;
            }
        }
    }
        
    // part #1: Get NAND CID, set up TWL/CTR counter
    u32 NandCid[4];
    u8 shasum[32];
    
    sdmmc_sdcard_init();
    sdmmc_get_cid(1, NandCid);
    sha_quick(shasum, (u8*) NandCid, 16, SHA256_MODE);
    memcpy(CtrNandCtr, shasum, 16);
    sha_quick(shasum, (u8*) NandCid, 16, SHA1_MODE);
    for(u32 i = 0; i < 16; i++) // little endian and reversed order
        TwlNandCtr[i] = shasum[15-i];
    
    // part #2: TWL KEY
    // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
    if (IS_A9LH) { // only for a9lh
        u32* TwlCustId = (u32*) (0x01FFB808);
        u8 TwlKeyX[16] __attribute__((aligned(32)));
        u8 TwlKeyY[16] __attribute__((aligned(32)));
        
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
    
    // part #3: CTRNAND N3DS KEY / AGBSAVE CMAC KEY
    // thanks AuroraWright and Gelex for advice on this
    // see: https://github.com/AuroraWright/Luma3DS/blob/master/source/crypto.c#L347
    
    // keyY 0x05 is encrypted @0x0EB014 in the FIRM90
    // keyY 0x05 is encrypted @0x0EB24C in the FIRM81
    if ((LoadKeyYFromP9(slot0x05KeyY, slot0x05KeyY_sha256, 0x0EB014, 0x05) != 0) &&
        (LoadKeyYFromP9(slot0x05KeyY, slot0x05KeyY_sha256, 0x0EB24C, 0x05) != 0))
        LoadKeyFromFile(slot0x05KeyY, 0x05, 'Y', NULL);
    
    // keyY 0x24 is encrypted @0x0E62DC in the FIRM90
    // keyY 0x24 is encrypted @0x0E6514 in the FIRM81
    if ((LoadKeyYFromP9(NULL, slot0x24KeyY_sha256, 0x0E62DC, 0x24) != 0) &&
        (LoadKeyYFromP9(NULL, slot0x24KeyY_sha256, 0x0E6514, 0x24) != 0))
        LoadKeyFromFile(NULL, 0x24, 'Y', NULL);
    
    return true;
}

bool CheckSlot0x05Crypto(void)
{
    // step #1 - check the slot0x05KeyY SHA-256
    if (sha_cmp(slot0x05KeyY_sha256, slot0x05KeyY, 16, SHA256_MODE) == 0)
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
    u8 buffer[0x200];
    ReadNandSectors(buffer, SECTOR_SECRET, 1, 0x11, NAND_SYSNAND);
    return (sha_cmp(KEY95_SHA256, buffer, 16, SHA256_MODE) == 0);
}

void CryptNand(u8* buffer, u32 sector, u32 count, u32 keyslot)
{
    u32 mode = (keyslot != 0x03) ? AES_CNT_CTRNAND_MODE : AES_CNT_TWLNAND_MODE; // somewhat hacky
    u8 ctr[16] __attribute__((aligned(32)));
    u32 blocks = count * (0x200 / 0x10);
    
    // copy NAND CTR and increment it
    memcpy(ctr, (keyslot != 0x03) ? CtrNandCtr : TwlNandCtr, 16); // hacky again
    add_ctr(ctr, sector * (0x200 / 0x10));
    
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
    ecb_decrypt((void*) buffer, (void*) buffer, 0x200 / AES_BLOCK_SIZE, mode);
}

int ReadNandBytes(u8* buffer, u64 offset, u64 count, u32 keyslot, u32 nand_src)
{
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for ReadNandSectors(...)
        return ReadNandSectors(buffer, offset / 0x200, count / 0x200, keyslot, nand_src);
    } else { // misaligned data -> -___-
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            errorcode = ReadNandSectors(l_buffer, offset / 0x200, 1, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
            memcpy(buffer, l_buffer + 0x200 - offset_fix, min(offset_fix, count));
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is read
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = ReadNandSectors(buffer, offset / 0x200, count / 0x200, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (offset + count) / 0x200, 1, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
            memcpy(buffer + count - count_fix, l_buffer, count_fix);
        }
        return errorcode;
    }
}

int WriteNandBytes(const u8* buffer, u64 offset, u64 count, u32 keyslot, u32 nand_dst)
{
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for WriteNandSectors(...)
        return WriteNandSectors(buffer, offset / 0x200, count / 0x200, keyslot, nand_dst);
    } else { // misaligned data -> -___-
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            errorcode = ReadNandSectors(l_buffer, offset / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer + 0x200 - offset_fix, buffer, min(offset_fix, count));
            errorcode = WriteNandSectors((const u8*) l_buffer, offset / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is written
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = WriteNandSectors(buffer, offset / 0x200, count / 0x200, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (offset + count) / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer, buffer + count - count_fix, count_fix);
            errorcode = WriteNandSectors((const u8*) l_buffer, (offset + count) / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
        }
        return errorcode;
    }
}

int ReadNandSectors(u8* buffer, u32 sector, u32 count, u32 keyslot, u32 nand_src)
{
    if (!count) return 0; // <--- just to be safe
    if (nand_src == NAND_EMUNAND) { // EmuNAND
        int errorcode = 0;
        if ((sector == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
            errorcode = sdmmc_sdcard_readsectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, buffer);
            if ((keyslot < 0x40) && (keyslot != 0x11) && !errorcode) CryptNand(buffer, 0, 1, keyslot);
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
    } else if (nand_src == NAND_ZERONAND) { // zero NAND (good for XORpads)
        memset(buffer, 0, count * 0x200);
    } else {
        return -1;
    }
    if ((keyslot == 0x11) && (sector == SECTOR_SECRET)) CryptSector0x96(buffer, false);
    else if (keyslot < 0x40) CryptNand(buffer, sector, count, keyslot);
    
    return 0;
}

int WriteNandSectors(const u8* buffer, u32 sector, u32 count, u32 keyslot, u32 nand_dst)
{
    // buffer must not be changed, so this is a little complicated
    for (u32 s = 0; s < count; s += (NAND_BUFFER_SIZE / 0x200)) {
        u32 pcount = min((NAND_BUFFER_SIZE/0x200), (count - s));
        memcpy(NAND_BUFFER, buffer + (s*0x200), pcount * 0x200);
        if ((keyslot == 0x11) && (sector == SECTOR_SECRET)) CryptSector0x96(NAND_BUFFER, true);
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

u32 GetNandNcsdMinSizeSectors(NandNcsdHeader* ncsd) // in sectors
{
    u32 nand_minsize = 1;
    for (u32 prt_idx = 0; prt_idx < 8; prt_idx++) {
        u32 prt_end = ncsd->partitions[prt_idx].offset + ncsd->partitions[prt_idx].size;
        if (prt_end > nand_minsize) nand_minsize = prt_end;
    }
    
    return nand_minsize;
}

u32 GetNandNcsdPartitionInfo(NandPartitionInfo* info, u32 type, u32 subtype, u32 index, NandNcsdHeader* ncsd)
{
    // safety / set keyslot
    if ((type == NP_TYPE_FAT) || (type > NP_TYPE_BONUS) || (subtype > NP_SUBTYPE_CTR_N)) return 1;
    info->keyslot = np_keyslots[type][subtype];
    
    // full (minimum) NAND "partition"
    if (type == NP_TYPE_NONE) {
        info->sector = 0x00;
        info->count = GetNandNcsdMinSizeSectors(ncsd);
        return 0;
    }
    
    // special, custom partition types, not in NCSD
    if (type >= NP_TYPE_NCSD) {
        if (type == NP_TYPE_NCSD) {
            info->sector = 0x00; // hardcoded
            info->count = 0x01;
        } else if (type == NP_TYPE_D0K3) {
            info->sector = SECTOR_D0K3; // hardcoded
            info->count = SECTOR_SECRET - info->sector;
        } else if (type == NP_TYPE_SECRET) {
            info->sector = SECTOR_SECRET;
            info->count = 0x01;
        } else if (type == NP_TYPE_BONUS) {
            info->sector = GetNandNcsdMinSizeSectors(ncsd);
            info->count = 0x00; // placeholder, actual size needs info from NAND chip
        } else return 1;
        return 0;
    }
    
    u32 prt_idx = 8;
    for (prt_idx = 0; prt_idx < 8; prt_idx++) {
        if ((ncsd->partitions_fs_type[prt_idx] != type) ||
            (ncsd->partitions_crypto_type[prt_idx] != subtype)) continue;
        if (index == 0) break;
        index--;
    }
    
    if (prt_idx >= 8) return 1; // not found
    info->sector = ncsd->partitions[prt_idx].offset;
    info->count = ncsd->partitions[prt_idx].size;
    
    return 0;
}

u32 GetNandPartitionInfo(NandPartitionInfo* info, u32 type, u32 subtype, u32 index, u32 nand_src)
{
    // check NAND NCSD integrity(!!!)
    // workaround for ZERONAND
    if (nand_src == NAND_ZERONAND) nand_src = NAND_SYSNAND;
    
    // find type & subtype in NCSD header
    u8 header[0x200];
    ReadNandSectors(header, 0x00, 1, 0xFF, nand_src);
    NandNcsdHeader* ncsd = (NandNcsdHeader*) header;
    if (((type == NP_TYPE_FAT) && (GetNandNcsdPartitionInfo(info, NP_TYPE_STD, subtype, 0, ncsd) != 0)) ||
        ((type != NP_TYPE_FAT) && (GetNandNcsdPartitionInfo(info, type, subtype, index, ncsd) != 0)))
        return 1; // not found
    
    // size of bonus partition
    if (type == NP_TYPE_BONUS) {
        info->count = GetNandSizeSectors(nand_src) - info->sector;
    } else if (type == NP_TYPE_FAT) { // FAT type specific stuff
        ReadNandSectors(header, info->sector, 1, info->keyslot, nand_src);
        MbrHeader* mbr = (MbrHeader*) header;
        if ((ValidateMbrHeader(mbr) != 0) || (index >= 4) ||
            (mbr->partitions[index].sector == 0) || (mbr->partitions[index].count == 0) ||
            (mbr->partitions[index].sector + mbr->partitions[index].count > info->count))
            return 1;
        info->sector += mbr->partitions[index].sector;
        info->count = mbr->partitions[index].count;
    }
    
    return 0;
}    

u32 CheckNandMbr(u8* mbr)
{
    if (memcmp(mbr + (0x200 - sizeof(twl_mbr)), twl_mbr, sizeof(twl_mbr)) == 0)
        return NAND_TYPE_TWL; // TWLNAND MBR (included in NAND header)
    else if (memcmp(mbr + (0x200 - sizeof(ctr_mbr_o3ds)), ctr_mbr_o3ds, sizeof(ctr_mbr_o3ds)) == 0)
        return NAND_TYPE_O3DS; // CTRNAND MBR (@0x05C980)
    else if (memcmp(mbr + (0x200 - sizeof(ctr_mbr_n3ds)), ctr_mbr_n3ds, sizeof(ctr_mbr_n3ds)) == 0)
        return NAND_TYPE_N3DS; // CTRNAND MBR (@0x05C980)
    else return 0;
}

u32 CheckNandHeader(u8* header)
{
    // TWL MBR check
    u8 header_dec[0x200];
    memcpy(header_dec, header, 0x200);
    CryptNand(header_dec, 0, 1, 0x03);
    if (CheckNandMbr(header_dec) != NAND_TYPE_TWL)
        return 0; // header does not belong to console
    
    // header type check
    if (memcmp(header + 0x100, nand_magic_n3ds, sizeof(nand_magic_n3ds)) == 0)
        return (IS_O3DS) ? 0 : NAND_TYPE_N3DS;
    else if (memcmp(header + 0x100, nand_magic_o3ds, sizeof(nand_magic_o3ds)) == 0)
        return NAND_TYPE_O3DS;
    
    return 0;
}

u32 CheckNandType(u32 nand_src)
{
    if (ReadNandSectors(NAND_BUFFER, 0, 1, 0xFF, nand_src) != 0)
        return 0;
    if (memcmp(NAND_BUFFER + 0x100, nand_magic_n3ds, sizeof(nand_magic_n3ds)) == 0) {
        return (IS_O3DS) ? 0 : NAND_TYPE_N3DS;
    } else if (memcmp(NAND_BUFFER + 0x100, nand_magic_o3ds, sizeof(nand_magic_o3ds)) == 0) {
        u8 magic[8] = {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20};
        if (ReadNandSectors(NAND_BUFFER, 0x5CAE5, 1, 0x04, nand_src) != 0)
            return 0;
        return ((IS_O3DS) || (memcmp(magic, NAND_BUFFER, 8) == 0)) ?
            NAND_TYPE_O3DS : NAND_TYPE_NO3DS;
    }
    
    return 0;
}

u64 GetNandSizeSectors(u32 nand_src)
{
    u32 sysnand_sectors = getMMCDevice(0)->total_size;
    if (nand_src == NAND_EMUNAND) { // for EmuNAND
        u32 partition_offset = GetPartitionOffsetSector("0:");
        u32 emunand_max_sectors = (partition_offset >= (emunand_base_sector + 1)) ? // +1 for safety
            partition_offset - (emunand_base_sector + 1) : 0;
        u32 emunand_min_sectors = (emunand_base_sector % 0x2000 == 0) ? sysnand_sectors : NAND_MIN_SECTORS;
        return (emunand_min_sectors > emunand_max_sectors) ? 0 : emunand_min_sectors;
    } else if (nand_src == NAND_IMGNAND) { // for images
        u32 img_sectors = (GetMountState() & IMG_NAND) ? GetMountSize() / 0x200 : 0;
        return (img_sectors >= sysnand_sectors) ? sysnand_sectors : (img_sectors >= NAND_MIN_SECTORS) ? NAND_MIN_SECTORS : 0;
    } else if (nand_src == NAND_SYSNAND) { // for SysNAND
        return sysnand_sectors;
    }
    
    return 0;
}

u64 GetNandUnusedSectors(u32 nand_src)
{
    return GetNandSizeSectors(nand_src) - NAND_MIN_SECTORS;
}

u32 GetLegitSector0x96(u8* sector)
{
    // secret sector already in buffer?
    if (sha_cmp(SECTOR_SHA256, sector, 0x200, SHA256_MODE) == 0)
        return 0;
    
    // search for valid secret sector in SysNAND / EmuNAND
    const u32 nand_src[] = { NAND_SYSNAND, NAND_EMUNAND };
    for (u32 i = 0; i < sizeof(nand_src) / sizeof(u32); i++) {
        ReadNandSectors(sector, SECTOR_SECRET, 1, 0x11, nand_src[i]);
        if (sha_cmp(SECTOR_SHA256, sector, 0x200, SHA256_MODE) == 0)
            return 0;
    }
    
    // no luck? try searching for a file
    const char* base[] = { INPUT_PATHS };
    for (u32 i = 0; i < (sizeof(base)/sizeof(char*)); i++) {
        char path[64];
        snprintf(path, 64, "%s/%s", base[i], SECTOR_NAME);
        if ((FileGetData(path, sector, 0x200, 0) == 0x200) &&
            (sha_cmp(SECTOR_SHA256, sector, 0x200, SHA256_MODE) == 0))
            return 0;
        snprintf(path, 64, "%s/%s", base[i], SECRET_NAME);
        if ((FileGetData(path, sector, 0x200, 0) == 0x200) &&
            (sha_cmp(SECTOR_SHA256, sector, 0x200, SHA256_MODE) == 0))
            return 0;
    }
    
    // failed if we arrive here
    return 1;
}

// OTP hash is 32 byte in size
u32 GetOtpHash(void* hash) {
    if (!CheckSector0x96Crypto()) return 1;
    memcpy(hash, OtpSha256, 0x20);
    return 0;
}

// NAND CID is 16 byte in size
u32 GetNandCid(void* cid) {
    sdmmc_get_cid(1, (u32*) cid);
    return 0;
}

bool CheckMultiEmuNand(void)
{
    // this only checks for the theoretical possibility
    return (GetPartitionOffsetSector("0:") >= (u64) (align(NAND_MIN_SECTORS + 1, 0x2000) * 2));
}

u32 InitEmuNandBase(bool reset)
{
    if (!reset) {
        u32 last_valid = emunand_base_sector;
        
        // legacy type multiNAND
        u32 legacy_sectors = (getMMCDevice(0)->total_size > 0x200000) ? 0x400000 : 0x200000;
        emunand_base_sector += legacy_sectors - (emunand_base_sector % legacy_sectors);
        if (GetNandSizeSectors(NAND_EMUNAND) && CheckNandType(NAND_EMUNAND))
            return emunand_base_sector; // GW type EmuNAND
        emunand_base_sector++;
        if (GetNandSizeSectors(NAND_EMUNAND) && CheckNandType(NAND_EMUNAND))
            return emunand_base_sector; // RedNAND type EmuNAND
        
        // compact type multiNAND
        if (last_valid % 0x2000 <= 1) {
            u32 compact_sectors = align(NAND_MIN_SECTORS + 1, 0x2000);
            emunand_base_sector = last_valid + compact_sectors;
            if (GetNandSizeSectors(NAND_EMUNAND) && CheckNandType(NAND_EMUNAND))
                return emunand_base_sector;
        }
    }
    
    emunand_base_sector = 0x000000; // GW type EmuNAND
    if (!CheckNandType(NAND_EMUNAND))
        emunand_base_sector = 0x000001; // RedNAND type EmuNAND
    
    return emunand_base_sector;
}

u32 GetEmuNandBase(void)
{
    return emunand_base_sector;
}
