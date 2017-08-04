#include "nandutil.h"
#include "nand.h"
#include "firm.h"
#include "fatmbr.h"
#include "essentials.h" // for essential backup struct
#include "image.h"
#include "fsinit.h"
#include "fsperm.h"
#include "unittype.h"
#include "sha.h"
#include "ui.h"
#include "vff.h"


u32 ReadNandFile(FIL* file, void* buffer, u32 sector, u32 count, u32 keyslot) {
    u32 offset = sector * 0x200;
    u32 size = count * 0x200;
    UINT btr;
    if ((fvx_tell(file) != offset) && (fvx_lseek(file, offset) != FR_OK))
        return 1; // seek failed
    if ((fvx_read(file, buffer, size, &btr) != FR_OK) || (btr != size))
        return 1; // read failed
    if (keyslot < 0x40) CryptNand(buffer, sector, count, keyslot);
    return 0;
}

u32 BuildEssentialBackup(const char* path, EssentialBackup* essential) {
    // prepare essential backup struct
    ExeFsFileHeader filelist[] = {
        { "nand_hdr", 0x0000, 0x200 },
        { "secinfo" , 0x0200, 0x111 },
        { "movable" , 0x0400, 0x140 },
        { "frndseed", 0x0600, 0x110 },
        { "nand_cid", 0x0800, 0x010 },
        { "otp"     , 0x0A00, 0x100 },
        { "hwcal0"  , 0x0C00, 0x9D0 },
        { "hwcal1"  , 0x1600, 0x9D0 }
    };
    memset(essential, 0, sizeof(EssentialBackup));
    memcpy(essential, filelist, sizeof(filelist));
    
    // backup current mount path, mount new path
    char path_store[256] = { 0 };
    char* path_bak = NULL;
    strncpy(path_store, GetMountPath(), 256);
    if (*path_store) path_bak = path_store;
    if (!InitImgFS(path)) {
        InitImgFS(path_bak);
        return 1;
    }
    
    // read four files
    ExeFsFileHeader* files = essential->header.files;
    if ((fvx_qread("I:/nand_hdr.bin", &(essential->nand_hdr), 0, 0x200, (UINT*) &(files[0].size)) != FR_OK) ||
        ((fvx_qread("7:/rw/sys/SecureInfo_A", &(essential->secinfo), 0, 0x200, (UINT*) &(files[1].size)) != FR_OK) &&
         (fvx_qread("7:/rw/sys/SecureInfo_B", &(essential->secinfo), 0, 0x200, (UINT*) &(files[1].size)) != FR_OK)) ||
        (fvx_qread("7:/private/movable.sed", &(essential->movable), 0, 0x200, (UINT*) &(files[2].size)) != FR_OK) ||
        ((fvx_qread("7:/rw/sys/LocalFriendCodeSeed_B", &(essential->frndseed), 0, 0x200, (UINT*) &(files[3].size)) != FR_OK) &&
         (fvx_qread("7:/rw/sys/LocalFriendCodeSeed_A", &(essential->frndseed), 0, 0x200, (UINT*) &(files[3].size)) != FR_OK))) {
        InitImgFS(path_bak);
        return 1;
    }
    
    // HWCAL0.dat / HWCAL1.dat
    if ((fvx_qread("7:/ro/sys/HWCAL0.dat", &(essential->hwcal0), 0, 0x1000, (UINT*) &(files[6].size)) != FR_OK) ||
        (fvx_qread("7:/ro/sys/HWCAL1.dat", &(essential->hwcal1), 0, 0x1000, (UINT*) &(files[7].size)) != FR_OK)) {
        memset(&(filelist[6]), 0, 2 * sizeof(ExeFsFileHeader));
    }
    
    // mount original file
    InitImgFS(path_bak);
    
    // fill nand cid / otp hash
    if (GetNandCid(&(essential->nand_cid)) != 0) return 1;
    if (!IS_UNLOCKED) memset(&(filelist[5]), 0, 3 * sizeof(ExeFsFileHeader));
    else memcpy(&(essential->otp), (u8*) 0x10012000, 0x100);
    
    // calculate hashes
    for (u32 i = 0; i < 8 && *(filelist[i].name); i++) 
        sha_quick(essential->header.hashes[9-i],
            ((u8*) essential) + files[i].offset + sizeof(ExeFsHeader),
            files[i].size, SHA256_MODE);
    
    return 0;
}

u32 CheckEmbeddedBackup(const char* path) {
    EssentialBackup* essential = (EssentialBackup*) TEMP_BUFFER;
    EssentialBackup* embedded = (EssentialBackup*) (TEMP_BUFFER + sizeof(EssentialBackup));
    UINT btr;
    if ((BuildEssentialBackup(path, essential) != 0) ||
        (fvx_qread(path, embedded, SECTOR_D0K3 * 0x200, sizeof(EssentialBackup), &btr) != FR_OK) ||
        (memcmp(embedded, essential, sizeof(EssentialBackup)) != 0))
        return 1;
    return 0;
}

u32 EmbedEssentialBackup(const char* path) {
    EssentialBackup* essential = (EssentialBackup*) TEMP_BUFFER;
    UINT btw;
    // leaving out the write permissions check here, it's okay
    if ((BuildEssentialBackup(path, essential) != 0) ||
        (ValidateNandNcsdHeader((NandNcsdHeader*) essential->nand_hdr) != 0) ||
        (fvx_qwrite(path, essential, SECTOR_D0K3 * 0x200, sizeof(EssentialBackup), &btw) != FR_OK) ||
        (btw != sizeof(EssentialBackup)))
        return 1;
    return 0;
}

u32 ValidateNandDump(const char* path) {
    NandPartitionInfo info;
    FIL file;
    
    // truncated path string
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // open file
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    // check NAND header
    NandNcsdHeader ncsd;
    if ((ReadNandFile(&file, &ncsd, 0, 1, 0xFF) != 0) || (ValidateNandNcsdHeader(&ncsd) != 0)) {
        ShowPrompt(false, "%s\nNCSD header is not valid", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // check size
    if (fvx_size(&file) < (GetNandNcsdMinSizeSectors(&ncsd) * 0x200)) {
        ShowPrompt(false, "%s\nNAND dump misses data", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // check TWL & CTR FAT partitions
    for (u32 i = 0; i < 2; i++) {
        char* section_type = (i) ? "CTR" : "MBR";
        if (i == 0) { // check TWL first, then CTR
            if (GetNandNcsdPartitionInfo(&info, NP_TYPE_STD, NP_SUBTYPE_TWL, 0, &ncsd) != 0) return 1;
        } else if ((GetNandNcsdPartitionInfo(&info, NP_TYPE_STD, NP_SUBTYPE_CTR, 0, &ncsd) != 0) &&
            (GetNandNcsdPartitionInfo(&info, NP_TYPE_STD, NP_SUBTYPE_CTR_N, 0, &ncsd) != 0)) return 1;
        MbrHeader mbr;
        if ((ReadNandFile(&file, &mbr, info.sector, 1, info.keyslot) != 0) ||
            (ValidateMbrHeader(&mbr) != 0)) {
            ShowPrompt(false, "%s\nError: %s MBR is corrupt", pathstr, section_type);
            fvx_close(&file);
            return 1; // impossible to happen
        }
        for (u32 p = 0; p < 4; p++) {
            u32 p_sector = mbr.partitions[p].sector;
            u8 fat[0x200];
            if (!p_sector) continue;
            if ((ReadNandFile(&file, fat, info.sector + p_sector, 1, info.keyslot) != 0) ||
                (ValidateFatHeader(fat) != 0)) {
                ShowPrompt(false, "%s\nError: %s partition%u is corrupt", pathstr, section_type, p);
                fvx_close(&file);
                return 1;
            }
        }
    }
    
    // check FIRMs (at least one FIRM must be valid)
    // check all 8 firms, also check if ARM9 & ARM11 entrypoints are available
    for (u32 f = 0; f <= 8; f++) {
        FirmHeader firm;
        if (GetNandNcsdPartitionInfo(&info, NP_TYPE_FIRM, NP_SUBTYPE_CTR, f, &ncsd) != 0) {
            ShowPrompt(false, "%s\nNo valid FIRM found", pathstr);
            fvx_close(&file);
            return 1;
        }
        if ((ReadNandFile(&file, &firm, info.sector, 1, info.keyslot) != 0) ||
            (ValidateFirmHeader(&firm, 0) != 0) || (getbe32(firm.dec_magic) != 0) || // decrypted firms are not allowed
            (!firm.entry_arm9) || (!firm.entry_arm11))  // arm9 / arm11 entry points must be there
            continue;
        // hash verify all available sections
        u32 s;
        for (s = 0; s < 4; s++) {
            FirmSectionHeader* section = firm.sections + s;
            u32 sector = info.sector + (section->offset / 0x200);
            u32 count = section->size / 0x200;
            if (!count) continue;
            sha_init(SHA256_MODE);
            // relies on sections being aligned to sectors
            for (u32 c = 0; c < count; c += MAIN_BUFFER_SIZE / 0x200) {
                u32 read_sectors = min(MAIN_BUFFER_SIZE / 0x200, (count - c));
                ReadNandFile(&file, MAIN_BUFFER, sector + c, read_sectors, info.keyslot);
                sha_update(MAIN_BUFFER, read_sectors * 0x200);
            }
            u8 hash[0x20];
            sha_get(hash);
            if (memcmp(hash, section->hash, 0x20) != 0) break;
        }
        if (s >= 4) break; // valid FIRM found
    }
    fvx_close(&file);
    
    return 0;
}

u32 SafeRestoreNandDump(const char* path) {
    if ((ValidateNandDump(path) != 0) && // NAND dump validation
        !ShowPrompt(true, "Error: NAND dump is corrupt.\nStill continue?"))
        return 1;
    if (!IS_A9LH) {
        ShowPrompt(false, "Error: B9S/A9LH not detected.");
        return 1;
    }
    
    if (!ShowUnlockSequence(5, "!WARNING!\n \nProceeding will overwrite the\nSysNAND with the provided dump.\n \n(B9S/A9LH will be left intact.)"))
        return 1;
    if (!SetWritePermissions(PERM_SYS_LVL1, true)) return 1;
    
    // build essential backup from NAND
    EssentialBackup* essential = (EssentialBackup*) TEMP_BUFFER;
    if (BuildEssentialBackup("1:/nand.bin", essential) != 0)
        memset(essential, 0, sizeof(EssentialBackup));
    
    // open file, get size
    FIL file;
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    u32 fsize = fvx_size(&file);
    
    // get NCSD headers from image and SysNAND
    NandNcsdHeader ncsd_loc, ncsd_img;
    MbrHeader twl_mbr_img;
    if ((ReadNandFile(&file, &ncsd_img, 0, 1, 0xFF) != 0) ||
        (ReadNandFile(&file, &twl_mbr_img, 0, 1, 0x03) != 0) ||
        (ReadNandSectors((u8*) &ncsd_loc, 0, 1, 0xFF, NAND_SYSNAND) != 0)) {
        fvx_close(&file);
        return 1;
    }
    
    // compare NCSD header partitioning
    bool header_inject = false;
    if (memcmp(&ncsd_loc, &ncsd_img, sizeof(NandNcsdHeader)) != 0) {
        for (int p = -1; p <= 8; p++) {
            NandPartitionInfo info_loc = { 0 };
            NandPartitionInfo info_img = { 0 };
            u32 idx = (p < 0) ? 0 : p;
            u32 type = (p < 0) ? NP_TYPE_SECRET : NP_TYPE_FIRM;
            u32 subtype = (p < 0) ? NP_SUBTYPE_CTR_N : NP_SUBTYPE_CTR;
            bool np_loc = (GetNandNcsdPartitionInfo(&info_loc, type, subtype, idx, &ncsd_loc) == 0);
            bool np_img = (GetNandNcsdPartitionInfo(&info_img, type, subtype, idx, &ncsd_img) == 0);
            if (!np_loc && !np_img) {
                if (p >= 1) header_inject = true;
                break;
            }
            if ((np_loc != np_img) || (memcmp(&info_loc, &info_img, sizeof(NandPartitionInfo)) != 0))
                break;
        }
        if (!header_inject || (ValidateNandNcsdHeader(&ncsd_img) != 0) || (ValidateMbrHeader(&twl_mbr_img) != 0)) {
            ShowPrompt(false, "Image NCSD corrupt or customized,\nsafe restore is not possible!");
            fvx_close(&file);
            return 1;
        }
    }
    
    // additional warning for elevated write permissions
    if (header_inject) {
        if (!ShowPrompt(true, "!WARNING!\n \nNCSD differs between image and local,\nelevated write permissions required\n \nProceed on your own risk?") ||
            !SetWritePermissions(PERM_SYS_LVL3, true)) {
            fvx_close(&file);
            return 1;
        }
    }
    
    // main processing loop
    u32 ret = 0;
    u32 sector0 = 1; // start at the sector after NCSD
    if (!ShowProgress(0, 0, path)) ret = 1;
    for (int p = -1; p < 8; p++) {
        NandPartitionInfo np_info;
        u32 idx = (p < 0) ? 0 : p;
        u32 type = (p < 0) ? NP_TYPE_SECRET : NP_TYPE_FIRM;
        u32 subtype = (p < 0) ? NP_SUBTYPE_CTR_N : NP_SUBTYPE_CTR;
        u32 sector1 = (GetNandNcsdPartitionInfo(&np_info, type, subtype, idx, &ncsd_loc) == 0) ? np_info.sector : fsize / 0x200;
        for (u32 s = sector0; (s < sector1) && (ret == 0); s += MAIN_BUFFER_SIZE / 0x200) {
            u32 count = min(MAIN_BUFFER_SIZE / 0x200, (sector1 - s));
            if (ReadNandFile(&file, MAIN_BUFFER, s, count, 0xFF)) ret = 1;
            if (WriteNandSectors(MAIN_BUFFER, s, count, 0xFF, NAND_SYSNAND)) ret = 1;
            if (!ShowProgress(s + count, fsize / 0x200, path)) ret = 1;
        }
        if (sector1 == fsize / 0x200) break; // at file end
        sector0 = np_info.sector + np_info.count;
    }
    fvx_close(&file);
    
    // NCSD header inject, should only be required with 2.1 local NANDs on N3DS
    if (header_inject && (ret == 0) &&
        (WriteNandSectors((u8*) &ncsd_img, 0, 1, 0xFF, NAND_SYSNAND) != 0))
        ret = 1;
    
    // inject essential backup to NAND
    WriteNandSectors((u8*) essential, ESSENTIAL_SECTOR, (sizeof(EssentialBackup) + 0x1FF) / 0x200, 0xFF, NAND_SYSNAND);
    
    return ret;
}