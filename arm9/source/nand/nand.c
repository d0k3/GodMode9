#include "nand.h"
#include "fsdrive.h"
#include "unittype.h"
#include "aes.h"
#include "sha.h"
#include "fatmbr.h"
#include "sdmmc.h"
#include "image.h"
#include "memmap.h"


#define KEY95_SHA256    ((IS_DEVKIT) ? slot0x11Key95dev_sha256 : slot0x11Key95_sha256)
#define SECTOR_SHA256   ((IS_DEVKIT) ? sector0x96dev_sha256 : sector0x96_sha256)

// see: https://www.3dbrew.org/wiki/NCSD#NCSD_header
static const u32 np_keyslots[10][4] = { // [NP_TYPE][NP_SUBTYPE]
    { 0xFF, 0xFF, 0xFF, 0xFF }, // none
    { 0xFF, 0x03, 0x04, 0x05 }, // standard
    { 0xFF, 0x03, 0x04, 0x05 }, // FAT (custom, not in NCSD)
    { 0xFF, 0xFF, 0x06, 0xFF }, // FIRM
    { 0xFF, 0xFF, 0x07, 0xFF }, // AGBSAVE
    { 0xFF, 0xFF, 0xFF, 0xFF }, // NCSD (custom)
    { 0xFF, 0xFF, 0xFF, 0xFF }, // D0K3 (custom)
    { 0xFF, 0xFF, 0xFF, 0xFF }, // KEYDB (custom)
    { 0xFF, 0xFF, 0xFF, 0x11 }, // SECRET (custom)
    { 0xFF, 0xFF, 0xFF, 0xFF }  // BONUS (custom)
};

static u8 slot0x05KeyY[0x10] = { 0x00 }; // need to load this from FIRM0 / external file
static const u8 slot0x05KeyY_sha256[0x20] = { // hash for slot0x05KeyY (16 byte)
    0x98, 0x24, 0x27, 0x14, 0x22, 0xB0, 0x6B, 0xF2, 0x10, 0x96, 0x9C, 0x36, 0x42, 0x53, 0x7C, 0x86,
    0x62, 0x22, 0x5C, 0xFD, 0x6F, 0xAE, 0x9B, 0x0A, 0x85, 0xA5, 0xCE, 0x21, 0xAA, 0xB6, 0xC8, 0x4D
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

static u8 CtrNandCtr[16];
static u8 TwlNandCtr[16];
static u8 OtpSha256[32] = { 0 };
static bool Crypto0x96 = false;

static u32 emunand_base_sector = 0x000000;


bool GetOtp0x90(void* otp0x90, u32 len)
{
    // a short helper function for crypto setup outside of sighax
    u8 __attribute__((aligned(32))) otp_key[0x10];
    u8 __attribute__((aligned(32))) otp_iv[0x10];
    
    len = len - (len % 0x10);
    if (len > 0x90) len = 0x90;
    memcpy(otp0x90, (u8*) 0x01FFB800, len);
    if ((LoadKeyFromFile(otp_key, 0x11, 'N', "OTP") == 0) &&
        (LoadKeyFromFile(otp_iv, 0x11, 'I', "OTP") == 0)) {
        setup_aeskey(0x11, otp_key);
        use_aeskey(0x11);
        cbc_encrypt(otp0x90, otp0x90, len / 0x10, AES_CNT_TITLEKEY_ENCRYPT_MODE, otp_iv);
        return true;
    }
    
    return false;
}

bool InitNandCrypto(bool init_full)
{   
    // part #0: KeyX / KeyY for secret sector 0x96
    // on a9lh this MUST be run before accessing the SHA register in any other way
    if (IS_UNLOCKED) { // if OTP is unlocked
        // see: https://www.3dbrew.org/wiki/OTP_Registers
        sha_quick(OtpSha256, (u8*) __OTP_ADDR, 0x90, SHA256_MODE);
        Crypto0x96 = true; // valid 100% in that case, others need checking
    } else if (IS_A9LH) { // for a9lh
        // store the current SHA256 from register
        memcpy(OtpSha256, (void*) REG_SHAHASH, 32);
    }
    if (!CheckSector0x96Crypto()) { // if all else fails...
        u8 __attribute__((aligned(32))) otp0x90[0x90];
        if (GetOtp0x90(otp0x90, 0x90))
            sha_quick(OtpSha256, otp0x90, 0x90, SHA256_MODE);
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
    
    // part #2: TWL KEY (if not already set up)
    // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
    if (GetNandPartitionInfo(NULL, NP_TYPE_FAT, NP_SUBTYPE_TWL, 0, NAND_SYSNAND) != 0) {
        u64 TwlCustId = 0; // TWL customer ID (different for devkits)
        if (!IS_DEVKIT) TwlCustId = 0x80000000ULL | (*(vu64 *)0x01FFB808 ^ 0x8C267B7B358A6AFULL);
        else if (IS_UNLOCKED) TwlCustId = (*(vu64*)__OTP_ADDR);
        if (!TwlCustId && IS_DEVKIT) {
            u64 __attribute__((aligned(32))) otp0x10[2];
            if (GetOtp0x90(otp0x10, 0x10)) TwlCustId = *otp0x10;
        }
        
        if (TwlCustId) { // give up if TwlCustId not found
            u32 TwlKey0x03Y[4] __attribute__((aligned(32)));
            u32 TwlKey0x03X[4] __attribute__((aligned(32)));
            
            if (IS_DEVKIT) {
                TwlKey0x03X[1] = 0xEE7A4B1E;
                TwlKey0x03X[2] = 0xAF42C08B;
                LoadKeyFromFile(TwlKey0x03Y, 0x03, 'Y', NULL);
            } else {
                TwlKey0x03X[1] = *(vu32*)0x01FFD3A8; // "NINT"
                TwlKey0x03X[2] = *(vu32*)0x01FFD3AC; // "ENDO"
                memcpy(TwlKey0x03Y, (u8*) 0x01FFD3C8, 16);
            }
            
            TwlKey0x03X[0] = (u32) (TwlCustId>>0);
            TwlKey0x03X[3] = (u32) (TwlCustId>>32);
            TwlKey0x03Y[3] = 0xE1A00005;
            
            setup_aeskeyX(0x03, TwlKey0x03X);
            setup_aeskeyY(0x03, TwlKey0x03Y);
            use_aeskey(0x03);
            
            if (init_full) { // full init
                vu32 *RegKey0x01X = &REG_AESKEY0123[((0x30u * 0x01) + 0x10u)/4u];
                RegKey0x01X[2] = (u32) (TwlCustId>>32);
                RegKey0x01X[3] = (u32) (TwlCustId>>0);
                
                setup_aeskeyX(0x02, (u8*)0x01FFD398);
                if (IS_DEVKIT) {
                    u32 TwlKey0x02Y[4] __attribute__((aligned(32)));
                    LoadKeyFromFile(TwlKey0x02Y, 0x02, 'Y', NULL);
                    setup_aeskeyY(0x02, TwlKey0x02Y);
                } else setup_aeskeyY(0x02, (u8*)0x01FFD220);
                use_aeskey(0x02);
                
                if (IS_UNLOCKED)
                    (*(vu64*)0x10012100) = TwlCustId;
            }
        }
    }
    
    // part #3: CTRNAND N3DS KEY (if not set up)
    if (GetNandPartitionInfo(NULL, NP_TYPE_FAT, NP_SUBTYPE_CTR, 0, NAND_SYSNAND) != 0)
        LoadKeyFromFile(slot0x05KeyY, 0x05, 'Y', NULL);
    
    // part #4: AGBSAVE CMAC KEY (set up on A9LH and SigHax)
    if (init_full && (IS_A9LH || IS_SIGHAX))
        LoadKeyFromFile(NULL, 0x24, 'Y', NULL);
    
    // part #5: FULL INIT
    if (init_full) InitKeyDb(NULL);
    
    return true;
}

bool CheckSlot0x05Crypto(void)
{
    // step #1 - check the slot0x05KeyY SHA-256
    if (sha_cmp(slot0x05KeyY_sha256, slot0x05KeyY, 16, SHA256_MODE) == 0)
        return true;
    
    // step #2 - check actual presence of CTRNAND FAT
    if (GetNandPartitionInfo(NULL, NP_TYPE_STD, NP_SUBTYPE_CTR_N, 0, NAND_SYSNAND) == 0)
        return true;
    
    // failed if we arrive here
    return false;
}

bool CheckSector0x96Crypto(void)
{
    if (!Crypto0x96) {
        u8 buffer[0x200];
        ReadNandSectors(buffer, SECTOR_SECRET, 1, 0x11, NAND_SYSNAND);
        Crypto0x96 = (sha_cmp(KEY95_SHA256, buffer, 16, SHA256_MODE) == 0);
    }
    return Crypto0x96;
}

bool CheckGenuineNandNcsd(void)
{
    u8 gen_o3ds_hash[0x20] = {
        0xCD, 0xB8, 0x2B, 0xF3, 0xE0, 0xC7, 0xA3, 0xC7, 0x58, 0xDF, 0xDC, 0x4E, 0x27, 0x63, 0xBE, 0xE8,
        0xBE, 0x2B, 0x1D, 0xF4, 0xBA, 0x97, 0xAF, 0x7F, 0x19, 0x70, 0x99, 0xDB, 0x66, 0xF7, 0x2F, 0xD7
    };
    u8 gen_n3ds_hash[0x20] = {
        0x49, 0xB7, 0x4A, 0xF1, 0xFD, 0xB7, 0xCF, 0x5B, 0x76, 0x8F, 0xA2, 0x94, 0x0D, 0xB2, 0xB3, 0xE2,
        0xA4, 0xBD, 0x25, 0x03, 0x06, 0x03, 0x47, 0x0B, 0x24, 0x5A, 0x86, 0x6A, 0x43, 0x60, 0xBC, 0x84, 
    };
    
    u8 gen_hdr[0x100];
    if ((ReadNandBytes(gen_hdr, 0x100, 0x100, 0xFF, NAND_SYSNAND) != 0) ||
        (ReadNandBytes(gen_hdr + 0xBE, 0x1BE, 0x42, 0x03, NAND_SYSNAND) != 0))
        return false;
        
    return (sha_cmp((IS_O3DS) ? gen_o3ds_hash : gen_n3ds_hash, gen_hdr, 0x100, SHA256_MODE) == 0);
}

void CryptNand(void* buffer, u32 sector, u32 count, u32 keyslot)
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

void CryptSector0x96(void* buffer, bool encrypt)
{
    u32 mode = encrypt ? AES_CNT_ECB_ENCRYPT_MODE : AES_CNT_ECB_DECRYPT_MODE;
    
    // setup the key
    setup_aeskeyX(0x11, OtpSha256);
    setup_aeskeyY(0x11, OtpSha256 + 16);
    
    // decrypt the sector
    use_aeskey(0x11);
    ecb_decrypt((void*) buffer, (void*) buffer, 0x200 / AES_BLOCK_SIZE, mode);
}

int ReadNandBytes(void* buffer, u64 offset, u64 count, u32 keyslot, u32 nand_src)
{
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for ReadNandSectors(...)
        return ReadNandSectors(buffer, offset / 0x200, count / 0x200, keyslot, nand_src);
    } else { // misaligned data -> -___-
        u8* buffer8 = (u8*) buffer;
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            errorcode = ReadNandSectors(l_buffer, offset / 0x200, 1, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
            memcpy(buffer8, l_buffer + 0x200 - offset_fix, min(offset_fix, count));
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer8 += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is read
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = ReadNandSectors(buffer8, offset / 0x200, count / 0x200, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (offset + count) / 0x200, 1, keyslot, nand_src);
            if (errorcode != 0) return errorcode;
            memcpy(buffer8 + count - count_fix, l_buffer, count_fix);
        }
        return errorcode;
    }
}

int WriteNandBytes(const void* buffer, u64 offset, u64 count, u32 keyslot, u32 nand_dst)
{
    if (!(offset % 0x200) && !(count % 0x200)) { // aligned data -> simple case 
        // simple wrapper function for WriteNandSectors(...)
        return WriteNandSectors(buffer, offset / 0x200, count / 0x200, keyslot, nand_dst);
    } else { // misaligned data -> -___-
        u8* buffer8 = (u8*) buffer;
        u8 l_buffer[0x200];
        int errorcode = 0;
        if (offset % 0x200) { // handle misaligned offset
            u32 offset_fix = 0x200 - (offset % 0x200);
            errorcode = ReadNandSectors(l_buffer, offset / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer + 0x200 - offset_fix, buffer8, min(offset_fix, count));
            errorcode = WriteNandSectors((const u8*) l_buffer, offset / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            if (count <= offset_fix) return 0;
            offset += offset_fix;
            buffer8 += offset_fix;
            count -= offset_fix;
        } // offset is now aligned and part of the data is written
        if (count >= 0x200) { // otherwise this is misaligned and will be handled below
            errorcode = WriteNandSectors(buffer8, offset / 0x200, count / 0x200, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
        }
        if (count % 0x200) { // handle misaligned count
            u32 count_fix = count % 0x200;
            errorcode = ReadNandSectors(l_buffer, (offset + count) / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
            memcpy(l_buffer, buffer8 + count - count_fix, count_fix);
            errorcode = WriteNandSectors((const u8*) l_buffer, (offset + count) / 0x200, 1, keyslot, nand_dst);
            if (errorcode != 0) return errorcode;
        }
        return errorcode;
    }
}

int ReadNandSectors(void* buffer, u32 sector, u32 count, u32 keyslot, u32 nand_src)
{   
    u8* buffer8 = (u8*) buffer;
    if (!count) return 0; // <--- just to be safe
    if (nand_src == NAND_EMUNAND) { // EmuNAND
        int errorcode = 0;
        if ((sector == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
            errorcode = sdmmc_sdcard_readsectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, buffer8);
            if ((keyslot < 0x40) && (keyslot != 0x11) && !errorcode) CryptNand(buffer8, 0, 1, keyslot);
            sector = 1;
            count--;
            buffer8 += 0x200;
        }
        errorcode = (!errorcode && count) ? sdmmc_sdcard_readsectors(emunand_base_sector + sector, count, buffer8) : errorcode;
        if (errorcode) return errorcode;
    } else if (nand_src == NAND_IMGNAND) { // ImgNAND
        int errorcode = ReadImageSectors(buffer8, sector, count);
        if (errorcode) return errorcode;
    } else if (nand_src == NAND_SYSNAND) { // SysNAND
        int errorcode = sdmmc_nand_readsectors(sector, count, buffer8);
        if (errorcode) return errorcode;   
    } else if (nand_src == NAND_ZERONAND) { // zero NAND (good for XORpads)
        memset(buffer8, 0, count * 0x200);
    } else {
        return -1;
    }
    if ((keyslot == 0x11) && (sector == SECTOR_SECRET)) CryptSector0x96(buffer8, false);
    else if (keyslot < 0x40) CryptNand(buffer8, sector, count, keyslot);
    
    return 0;
}

int WriteNandSectors(const void* buffer, u32 sector, u32 count, u32 keyslot, u32 nand_dst)
{
    // buffer must not be changed, so this is a little complicated
    void* nand_buffer = (void*) malloc(min(STD_BUFFER_SIZE, count * 0x200));
    if (!nand_buffer) return -1;
    int errorcode = 0;
    
    for (u32 s = 0; s < count; s += (STD_BUFFER_SIZE / 0x200)) {
        u32 pcount = min((STD_BUFFER_SIZE/0x200), (count - s));
        memcpy(nand_buffer, ((u8*) buffer) + (s*0x200), pcount * 0x200);
        if ((keyslot == 0x11) && (sector == SECTOR_SECRET)) CryptSector0x96(nand_buffer, true);
        else if (keyslot < 0x40) CryptNand(nand_buffer, sector + s, pcount, keyslot);
        if (nand_dst == NAND_EMUNAND) {
            if ((sector + s == 0) && (emunand_base_sector % 0x200000 == 0)) { // GW EmuNAND header handling
                errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + getMMCDevice(0)->total_size, 1, nand_buffer);
                if (!errorcode && (pcount > 1)) errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + 1, pcount - 1, ((u8*) nand_buffer) + 0x200);
            } else errorcode = sdmmc_sdcard_writesectors(emunand_base_sector + sector + s, pcount, nand_buffer);
        } else if (nand_dst == NAND_IMGNAND) {
            errorcode = WriteImageSectors(nand_buffer, sector + s, pcount);
        } else if (nand_dst == NAND_SYSNAND) {
            errorcode = sdmmc_nand_writesectors(sector + s, pcount, nand_buffer);
        } else {
            errorcode = -1;
        }
    }
    
    free(nand_buffer);
    return errorcode;
}

u32 ValidateSecretSector(u8* sector)
{
    return (sha_cmp(SECTOR_SHA256, sector, 0x200, SHA256_MODE) == 0) ? 0 : 1;
}

// shamelessly stolen from myself
// see: https://github.com/d0k3/GodMode9/blob/master/source/game/ncsd.c#L4
u32 ValidateNandNcsdHeader(NandNcsdHeader* header)
{
    u8 zeroes[16] = { 0 };
    if ((memcmp(header->magic, "NCSD", 4) != 0) || // check magic number
        (memcmp(header->partitions_fs_type, zeroes, 8) == 0) || header->mediaId) // prevent detection of cart NCSD images
        return 1;
    
    u32 data_units = 0;
    u32 firm_count = 0;
    for (u32 i = 0; i < 8; i++) {
        NandNcsdPartition* partition = header->partitions + i;
        u8 np_type = header->partitions_fs_type[i];
        if ((i == 0) && !partition->size) return 1; // first content must be there
        else if (!partition->size) continue;
        if (!np_type) return 1; // partition must have a type
        if (partition->offset < data_units)
            return 1; // overlapping partitions, failed
        data_units = partition->offset + partition->size;
        if (np_type == NP_TYPE_FIRM) firm_count++; // count firms
    }
    if (data_units > header->size) return 1;
    if (!firm_count) return 1; // at least one firm is required
     
    return 0;
}

u32 GetNandNcsdMinSizeSectors(NandNcsdHeader* ncsd)
{
    u32 nand_minsize = 0;
    for (u32 prt_idx = 0; prt_idx < 8; prt_idx++) {
        u32 prt_end = ncsd->partitions[prt_idx].offset + ncsd->partitions[prt_idx].size;
        if (prt_end > nand_minsize) nand_minsize = prt_end;
    }
    
    return nand_minsize;
}

u32 GetNandMinSizeSectors(u32 nand_src)
{
    NandNcsdHeader ncsd;
    if ((ReadNandSectors((u8*) &ncsd, 0, 1, 0xFF, nand_src) != 0) ||
        (ValidateNandNcsdHeader(&ncsd) != 0)) return 0;
    
    return GetNandNcsdMinSizeSectors(&ncsd);
}

u32 GetNandSizeSectors(u32 nand_src)
{
    u32 sysnand_sectors = getMMCDevice(0)->total_size;
    if (nand_src == NAND_SYSNAND) return sysnand_sectors; // for SysNAND
    
    u32 min_sectors = GetNandMinSizeSectors(nand_src);
    if (nand_src == NAND_EMUNAND) { // for EmuNAND
        u32 partition_offset = GetPartitionOffsetSector("0:");
        u32 emunand_max_sectors = (partition_offset >= (emunand_base_sector + 1)) ? // +1 for safety
            partition_offset - (emunand_base_sector + 1) : 0;
        u32 emunand_min_sectors = (emunand_base_sector % 0x2000 == 0) ? sysnand_sectors : min_sectors;
        if (!emunand_min_sectors) emunand_min_sectors = GetNandMinSizeSectors(NAND_SYSNAND);
        return (emunand_min_sectors > emunand_max_sectors) ? 0 : emunand_min_sectors;
    } else if (nand_src == NAND_IMGNAND) { // for images
        u32 img_sectors = (GetMountState() & IMG_NAND) ? GetMountSize() / 0x200 : 0;
        return (img_sectors >= min_sectors) ? img_sectors : 0;
    }
    
    return 0;
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
            info->sector = SECTOR_NCSD; // hardcoded
            info->count = COUNT_NCSD;
        } else if (type == NP_TYPE_D0K3) {
            info->sector = SECTOR_D0K3; // hardcoded
            info->count = COUNT_D0K3;
        } else if (type == NP_TYPE_KEYDB) {
            info->sector = SECTOR_KEYDB; // hardcoded
            info->count = COUNT_KEYDB;
        } else if (type == NP_TYPE_SECRET) {
            info->sector = SECTOR_SECRET; // hardcoded
            info->count = COUNT_SECRET;
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
    // workaround for info == NULL
    NandPartitionInfo dummy;
    if (!info) info = &dummy;
    
    // workaround for ZERONAND
    if (nand_src == NAND_ZERONAND) nand_src = NAND_SYSNAND;
    
    // find type & subtype in NCSD header
    u8 header[0x200];
    ReadNandSectors(header, 0x00, 1, 0xFF, nand_src);
    NandNcsdHeader* ncsd = (NandNcsdHeader*) header;
    if ((ValidateNandNcsdHeader(ncsd) != 0) ||
        ((type == NP_TYPE_FAT) && (GetNandNcsdPartitionInfo(info, NP_TYPE_STD, subtype, 0, ncsd) != 0)) ||
        ((type != NP_TYPE_FAT) && (GetNandNcsdPartitionInfo(info, type, subtype, index, ncsd) != 0)))
        return 1; // not found
    
    if (type == NP_TYPE_BONUS) { // size of bonus partition
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

bool CheckMultiEmuNand(void)
{
    // this only checks for the theoretical possibility
    u32 emunand_min_sectors = GetNandMinSizeSectors(NAND_EMUNAND);
    return (emunand_min_sectors && (GetPartitionOffsetSector("0:") >= (u64) (align(emunand_min_sectors + 1, 0x2000) * 2)));
}

u32 AutoEmuNandBase(bool reset)
{
    if (!reset) {
        u32 last_valid = emunand_base_sector;
        u32 emunand_min_sectors = GetNandMinSizeSectors(NAND_EMUNAND);
        
        // legacy type multiNAND
        u32 legacy_sectors = (getMMCDevice(0)->total_size > 0x200000) ? 0x400000 : 0x200000;
        emunand_base_sector += legacy_sectors - (emunand_base_sector % legacy_sectors);
        if (GetNandSizeSectors(NAND_EMUNAND) && (GetNandPartitionInfo(NULL, NP_TYPE_NCSD, NP_SUBTYPE_CTR, 0, NAND_EMUNAND) == 0))
            return emunand_base_sector; // GW type EmuNAND
        emunand_base_sector++;
        if (GetNandSizeSectors(NAND_EMUNAND) && (GetNandPartitionInfo(NULL, NP_TYPE_NCSD, NP_SUBTYPE_CTR, 0, NAND_EMUNAND) == 0))
            return emunand_base_sector; // RedNAND type EmuNAND
        
        // compact type multiNAND
        if (emunand_min_sectors && (last_valid % 0x2000 <= 1)) {
            u32 compact_sectors = align(emunand_min_sectors + 1, 0x2000);
            emunand_base_sector = last_valid + compact_sectors;
            if (GetNandSizeSectors(NAND_EMUNAND) && (GetNandPartitionInfo(NULL, NP_TYPE_NCSD, NP_SUBTYPE_CTR, 0, NAND_EMUNAND) == 0))
                return emunand_base_sector;
        }
    }
    
    emunand_base_sector = 0x000000; // GW type EmuNAND
    if (GetNandPartitionInfo(NULL, NP_TYPE_NCSD, NP_SUBTYPE_CTR, 0, NAND_EMUNAND) != 0)
        emunand_base_sector = 0x000001; // RedNAND type EmuNAND
    
    return emunand_base_sector;
}

u32 GetEmuNandBase(void)
{
    return emunand_base_sector;
}

u32 SetEmuNandBase(u32 base_sector)
{
    return (emunand_base_sector = base_sector);
}
