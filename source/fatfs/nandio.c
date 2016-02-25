#include "platform.h"
#include "fatfs/aes.h"
#include "fatfs/nandio.h"
#include "fatfs/sdmmc.h"

// see: http://3dbrew.org/wiki/Flash_Filesystem
static PartitionInfo partitions[] = {
    { "TWLN",    {0xE9, 0x00, 0x00, 0x54, 0x57, 0x4C, 0x20, 0x20}, 0x00012E00, 0x08FB5200, 0x3, AES_CNT_TWLNAND_MODE },
    { "TWLP",    {0xE9, 0x00, 0x00, 0x54, 0x57, 0x4C, 0x20, 0x20}, 0x09011A00, 0x020B6600, 0x3, AES_CNT_TWLNAND_MODE },
    { "AGBSAVE", {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x0B100000, 0x00030000, 0x7, AES_CNT_CTRNAND_MODE },
    { "FIRM0",   {0x46, 0x49, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x00}, 0x0B130000, 0x00400000, 0x6, AES_CNT_CTRNAND_MODE },
    { "FIRM1",   {0x46, 0x49, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x00}, 0x0B530000, 0x00400000, 0x6, AES_CNT_CTRNAND_MODE },
    { "CTRNAND", {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}, 0x0B95CA00, 0x2F3E3600, 0x4, AES_CNT_CTRNAND_MODE }, // O3DS
    { "CTRNAND", {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}, 0x0B95AE00, 0x41D2D200, 0x5, AES_CNT_CTRNAND_MODE }  // N3DS
};

static u32 emunand_header = 0;
static u32 emunand_offset = 0;


u32 CheckEmuNand(void)
{
    u8 header[0x200];
    u32 nand_size_sectors = getMMCDevice(0)->total_size;
    u32 multi_sectors = (GetUnitPlatform() == PLATFORM_3DS) ? EMUNAND_MULTI_OFFSET_O3DS : EMUNAND_MULTI_OFFSET_N3DS;
    u32 ret = EMUNAND_NOT_READY;

    // check the MBR for presence of a hidden partition
    sdmmc_sdcard_readsectors(0, 1, header);
    u32 hidden_sectors = getle32(header + 0x1BE + 0x8);
    
    for (u32 offset_sector = 0; offset_sector + nand_size_sectors < hidden_sectors; offset_sector += multi_sectors) {
        // check for Gateway type EmuNAND
        sdmmc_sdcard_readsectors(offset_sector + nand_size_sectors, 1, header);
        if (memcmp(header + 0x100, "NCSD", 4) == 0) {
            ret |= EMUNAND_GATEWAY << (2 * (offset_sector / multi_sectors)); 
            continue;
        }
        // check for RedNAND type EmuNAND
        sdmmc_sdcard_readsectors(offset_sector + 1, 1, header);
        if (memcmp(header + 0x100, "NCSD", 4) == 0) {
            ret |= EMUNAND_REDNAND << (2 * (offset_sector / multi_sectors)); 
            continue;
        }
        // EmuNAND ready but not set up
       ret |= EMUNAND_READY << (2 * (offset_sector / multi_sectors)); 
    }
    
    return ret;
}

void SetNand(bool set_emunand, u32 base_sector)
{
    // no checks here AT ALL - be careful!!!
    if (set_emunand) {
        emunand_header = base_sector + getMMCDevice(0)->total_size;
        emunand_offset = base_sector;
    } else {
        emunand_header = 0;
        emunand_offset = 0;
    }
}

int ReadNandSectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if (emunand_header) {
        if (sector_no == 0) {
            int errorcode = sdmmc_sdcard_readsectors(emunand_header, 1, out);
            if (errorcode) return errorcode;
            sector_no = 1;
            numsectors--;
            out += 0x200;
        }
        return sdmmc_sdcard_readsectors(sector_no + emunand_offset, numsectors, out);
    } else return sdmmc_nand_readsectors(sector_no, numsectors, out);
}

int WriteNandSectors(u32 sector_no, u32 numsectors, u8 *in)
{
    if (emunand_header) {
        if (sector_no == 0) {
            int errorcode = sdmmc_sdcard_writesectors(emunand_header, 1, in);
            if (errorcode) return errorcode;
            sector_no = 1;
            numsectors--;
            in += 0x200;
        }
        return sdmmc_sdcard_writesectors(sector_no + emunand_offset, numsectors, in);
    } else return sdmmc_nand_writesectors(sector_no, numsectors, in);
}

u32 GetNandSize(void)
{
    return getMMCDevice(0)->total_size * NAND_SECTOR_SIZE;
}

PartitionInfo* GetPartitionInfo(u32 partition_id)
{
    u32 partition_num = 0;
    
    if (partition_id == P_CTRNAND) {
        partition_num = (GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6;
    } else {
        for(; !(partition_id & (1<<partition_num)) && (partition_num < 32); partition_num++);
    }
    
    return (partition_num >= 32) ? NULL : &(partitions[partition_num]);
}

u32 GetNandCtr(u8* ctr, u32 offset)
{
    // static const char* versions[] = {"4.x", "5.x", "6.x", "7.x", "8.x", "9.x"};
    static const u8* version_ctrs[] = {
        (u8*)0x080D7CAC,
        (u8*)0x080D858C,
        (u8*)0x080D748C,
        (u8*)0x080D740C,
        (u8*)0x080D74CC,
        (u8*)0x080D794C
    };
    static const u32 version_ctrs_len = sizeof(version_ctrs) / sizeof(u32);
    static u8* ctr_start = NULL;
    
    if (ctr_start == NULL) {
        for (u32 i = 0; i < version_ctrs_len; i++) {
            if (*(u32*)version_ctrs[i] == 0x5C980) {
                ctr_start = (u8*) version_ctrs[i] + 0x30;
            }
        }
        
        // If value not in previous list start memory scanning (test range)
        if (ctr_start == NULL) {
            for (u8* c = (u8*) 0x080D8FFF; c > (u8*) 0x08000000; c--) {
                if (*(u32*)c == 0x5C980 && *(u32*)(c + 1) == 0x800005C9) {
                    ctr_start = c + 0x30;
                    break;
                }
            }
        }
        
        if (ctr_start == NULL) {
            return 1;
        }
    }
    
    // the ctr is stored backwards in memory
    if (offset >= 0x0B100000) { // CTRNAND/AGBSAVE region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + (0xF - i));
    } else { // TWL region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + 0x88 + (0xF - i));
    }
    
    // increment counter
    add_ctr(ctr, offset / 0x10);

    return 0;
}

u32 CryptBuffer(CryptBufferInfo *info)
{
    u8 ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, info->ctr, 16);

    u8* buffer = info->buffer;
    u32 size = info->size;
    u32 mode = info->mode;

    if (info->setKeyY) {
        u8 keyY[16] __attribute__((aligned(32)));
        memcpy(keyY, info->keyY, 16);
        setup_aeskeyY(info->keyslot, keyY);
        info->setKeyY = 0;
    }
    use_aeskey(info->keyslot);

    for (u32 i = 0; i < size; i += 0x10, buffer += 0x10) {
        set_ctr(ctr);
        if ((mode & (0x7 << 27)) == AES_CBC_DECRYPT_MODE)
            memcpy(ctr, buffer, 0x10);
        aes_decrypt((void*) buffer, (void*) buffer, 1, mode);
        if ((mode & (0x7 << 27)) == AES_CBC_ENCRYPT_MODE)
            memcpy(ctr, buffer, 0x10);
        else if ((mode & (0x7 << 27)) == AES_CTR_MODE)
            add_ctr(ctr, 0x1);
    }

    memcpy(info->ctr, ctr, 16);
    
    return 0;
}

u32 DecryptNandToMem(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    CryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(GetNandCtr(info.ctr, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    ReadNandSectors(start_sector, n_sectors, buffer);
    CryptBuffer(&info);

    return 0;
}

u32 EncryptMemToNand(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    CryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(GetNandCtr(info.ctr, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    CryptBuffer(&info);
    WriteNandSectors(start_sector, n_sectors, buffer);

    return 0;
}
