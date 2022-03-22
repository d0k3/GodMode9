#include "nandutil.h"
#include "nandcmac.h"
#include "nand.h"
#include "firm.h"
#include "fatmbr.h"
#include "gba.h"
#include "fs.h"
#include "ui.h"
#include "sdmmc.h"
#include "sha.h"
#include "sighax.h"
#include "keydb.h"  // for perfect keydb hash and length
#include "essentials.h" // for essential backup struct
#include "unittype.h"
#include "memmap.h"


static const u8 twl_mbr_std[0x42] = {
    0x00, 0x04, 0x18, 0x00, 0x06, 0x01, 0xA0, 0x3F, 0x97, 0x00, 0x00, 0x00, 0xA9, 0x7D, 0x04, 0x00,
    0x00, 0x04, 0x8E, 0x40, 0x06, 0x01, 0xA0, 0xC3, 0x8D, 0x80, 0x04, 0x00, 0xB3, 0x05, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xAA
};


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
    path_store[255] = '\0';
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
    sdmmc_get_cid(1, (u32*) (void*) &(essential->nand_cid));
    if (!IS_UNLOCKED) memset(&(filelist[5]), 0, 3 * sizeof(ExeFsFileHeader));
    else memcpy(&(essential->otp), (u8*) __OTP_ADDR, 0x100);

    // calculate hashes
    for (u32 i = 0; i < 8 && *(filelist[i].name); i++)
        sha_quick(essential->header.hashes[9-i],
            ((u8*) essential) + files[i].offset + sizeof(ExeFsHeader),
            files[i].size, SHA256_MODE);

    return 0;
}

u32 CheckEmbeddedBackup(const char* path) {
    EssentialBackup* essential = (EssentialBackup*) malloc(sizeof(EssentialBackup));
    EssentialBackup* embedded = (EssentialBackup*) malloc(sizeof(EssentialBackup));

    if (!essential || !embedded || (BuildEssentialBackup(path, essential) != 0) ||
        (fvx_qread(path, embedded, SECTOR_D0K3 * 0x200, sizeof(EssentialBackup), NULL) != FR_OK) ||
        (memcmp(embedded, essential, sizeof(EssentialBackup)) != 0)) {
        free(essential);
        free(embedded);
        return 1;
    }

    free(essential);
    free(embedded);
    return 0;
}

u32 EmbedEssentialBackup(const char* path) {
    EssentialBackup* essential = (EssentialBackup*) malloc(sizeof(EssentialBackup));
    if (!essential) return 1;

    // leaving out the write permissions check here, it's okay
    if ((BuildEssentialBackup(path, essential) != 0) ||
        (ValidateNandNcsdHeader((void*)essential->nand_hdr) != 0) ||
        (fvx_qwrite(path, essential, SECTOR_D0K3 * 0x200, sizeof(EssentialBackup), NULL) != FR_OK)) {
        free(essential);
        return 1;
    }

    free(essential);
    return 0;
}

u32 DumpGbaVcSavegameBuffered(const char* path, void* buffer) {
    AgbSaveHeader* agbsave = (AgbSaveHeader*) buffer;
    u8* savegame = (u8*) (agbsave + 1);

    // read full AGBsave to memory
    if ((fvx_qread(path, agbsave, 0, sizeof(AgbSaveHeader), NULL) != FR_OK) || (ValidateAgbSaveHeader(agbsave) != 0) ||
        (fvx_qread(path, savegame, sizeof(AgbSaveHeader), agbsave->save_size, NULL) != FR_OK)) return 1; // not a proper AGBSAVE file

    // byteswap for eeprom type saves (512 byte / 8 kB)
    if ((agbsave->save_size == GBASAVE_EEPROM_512) || (agbsave->save_size == GBASAVE_EEPROM_8K)) {
        for (u8* ptr = savegame; (ptr - savegame) < (int) agbsave->save_size; ptr += 8)
            *(u64*) (void*) ptr = getbe64(ptr);
    }

    // ensure the output dir exists
    if (fvx_rmkdir(OUTPUT_PATH) != FR_OK)
        return 1;

    // generate output path
    char path_vcsav[64];
    snprintf(path_vcsav, 64, OUTPUT_PATH "/%016llX.gbavc.sav", agbsave->title_id);
    if (fvx_qwrite(path_vcsav, savegame, 0, agbsave->save_size, NULL) != FR_OK) return 1; // write fail

    return 0;
}

u32 DumpGbaVcSavegame(const char* path) {
    u8* buffer = (u8*) malloc(AGBSAVE_MAX_SIZE);
    if (!buffer) {
        ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
        return 1;
    }

    u32 ret = DumpGbaVcSavegameBuffered(path, buffer);
    free(buffer);
    return ret;
}

u32 InjectGbaVcSavegameBuffered(const char* path, const char* path_vcsave, void* buffer) {
    AgbSaveHeader* agbsave = (AgbSaveHeader*) buffer;
    u8* savegame = (u8*) (agbsave + 1);

    // basic sanity checks for path_vcsave
    FILINFO fno;
    char* ext = strrchr(path_vcsave, '.');
    if (!ext || ((strncasecmp(ext+1, "sav", 4) != 0) && (strncasecmp(ext+1, "srm", 4) != 0) &&
        (strncasecmp(ext+1, "SaveRAM", 8) != 0))) return 1; // bad extension
    if ((fvx_stat(path_vcsave, &fno) != FR_OK) || (!GBASAVE_VALID(fno.fsize) && !GBASAVE_VALID(fno.fsize - 16)))
        return 1; // bad size

    // read AGBsave header to memory
    if ((fvx_qread(path, agbsave, 0, sizeof(AgbSaveHeader), NULL) != FR_OK) ||
        (ValidateAgbSaveHeader(agbsave) != 0)) return 1; // not a proper header

    // read savegame to memory
    u32 inject_save_size = min(agbsave->save_size, fno.fsize);
    memset(savegame, 0xFF, agbsave->save_size); // pad with 0xFF
    if (fvx_qread(path_vcsave, savegame, 0, inject_save_size, NULL) != FR_OK) return 1;

    // byteswap for eeprom type saves (512 byte / 8 kB)
    if ((agbsave->save_size == GBASAVE_EEPROM_512) || (agbsave->save_size == GBASAVE_EEPROM_8K)) {
        for (u8* ptr = savegame; (ptr - savegame) < (int) inject_save_size; ptr += 8)
            *(u64*) (void*) ptr = getbe64(ptr);
    }

    // fix CMAC for NAND partition, rewrite AGBSAVE file
    u32 data_size = sizeof(AgbSaveHeader) + agbsave->save_size;
    if (FixAgbSaveCmac(agbsave, NULL, NULL) != 0) return 1;
    if (fvx_qwrite(path, agbsave, 0, data_size, NULL) != FR_OK) return 1; // write fail

    // fix CMAC for SD partition, take it over to SD
    if (strncasecmp(path, "S:/agbsave.bin", 256) == 0) {
        AgbSaveHeader agbsave_sd;
        char path_sd[64];
        u32 slot = 0;

        // get the SD save path, check SD save size
        snprintf(path_sd, 64, "A:/title/%08lx/%08lx/data/00000001.sav",
            getle32((u8*) &(agbsave->title_id) + 4), getle32((u8*) &(agbsave->title_id)));
        if ((fvx_stat(path_sd, &fno) != FR_OK) || (fno.fsize != max(AGBSAVE_MAX_SIZE, 2 * data_size)))
            return 1; // invalid / non-existant SD save

        // find out which slot to inject to
        if ((fvx_qread(path_sd, &agbsave_sd, 0, sizeof(AgbSaveHeader), NULL) == FR_OK) &&
            (ValidateAgbSaveHeader(&agbsave_sd) == 0) &&
            (agbsave->times_saved == agbsave_sd.times_saved))
            slot = data_size; // proper slot is bottom slot (otherwise it's the top slot)

        // inject next slot
        agbsave->times_saved++; // increase # of times saved
        if (FixAgbSaveCmac(agbsave, NULL, path_sd) != 0) return 1;
        if (fvx_qwrite(path_sd, agbsave, slot, data_size, NULL) != FR_OK) return 1; // write fail
    }

    // set CFG_BOOTENV to 0x7 so the save is taken over (not needed anymore)
    // https://www.3dbrew.org/wiki/CONFIG9_Registers#CFG9_BOOTENV
    // if (strncasecmp(path, "S:/agbsave.bin", 256) == 0) *(u32*) 0x10010000 = 0x7;

    return 0;
}

u32 InjectGbaVcSavegame(const char* path, const char* path_vcsave) {
    u8* buffer = (u8*) malloc(AGBSAVE_MAX_SIZE);
    if (!buffer) {
        ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
        return 1;
    }

    u32 ret = InjectGbaVcSavegameBuffered(path, path_vcsave, buffer);
    free(buffer);
    return ret;
}

u32 RebuildNandNcsdHeader(NandNcsdHeader* ncsd) {
    // signature (retail or dev)
    const u8* signature = (IS_DEVKIT) ? sig_nand_ncsd_dev : sig_nand_ncsd_retail;

    // encrypted TWL MBR
    u8 twl_mbr_data[0x200] = { 0 };
    u8* twl_mbr = twl_mbr_data + (0x200 - sizeof(twl_mbr_std));
    memcpy(twl_mbr, twl_mbr_std, sizeof(twl_mbr_std));
    CryptNand(twl_mbr_data, 0, 1, 0x03);

    // rebuild NAND header for console
    memset(ncsd, 0x00, sizeof(NandNcsdHeader));
    memcpy(ncsd->signature, signature, 0x100); // signature
    memcpy(ncsd->twl_mbr, twl_mbr, 0x42); // TWL MBR
    memcpy(ncsd->magic, "NCSD", 0x4); // magic number
    ncsd->size = (IS_O3DS) ? 0x200000 : 0x280000; // total size

    // TWL partition (0)
    ncsd->partitions_fs_type[0] = 0x01;
    ncsd->partitions_crypto_type[0] = 0x01;
    ncsd->partitions[0].offset = 0x000000;
    ncsd->partitions[0].size = 0x058800;

    // AGBSAVE partition (1)
    ncsd->partitions_fs_type[1] = 0x04;
    ncsd->partitions_crypto_type[1] = 0x02;
    ncsd->partitions[1].offset = 0x058800;
    ncsd->partitions[1].size = 0x000180;

    // FIRM0 partition (2)
    ncsd->partitions_fs_type[2] = 0x03;
    ncsd->partitions_crypto_type[2] = 0x02;
    ncsd->partitions[2].offset = 0x058980;
    ncsd->partitions[2].size = 0x002000;

    // FIRM1 partition (3)
    ncsd->partitions_fs_type[3] = 0x03;
    ncsd->partitions_crypto_type[3] = 0x02;
    ncsd->partitions[3].offset = 0x05A980;
    ncsd->partitions[3].size = 0x002000;

    // CTR partition (4)
    ncsd->partitions_fs_type[4] = 0x01;
    ncsd->partitions_crypto_type[4] = (IS_O3DS) ? 0x02 : 0x03;
    ncsd->partitions[4].offset = 0x05C980;
    ncsd->partitions[4].size = (IS_O3DS) ? 0x17AE80 : 0x20F680;

    // unknown stuff - whatever this is ¯\_(ツ)_/¯
    ncsd->unknown[0x25] = 0x04;
    ncsd->unknown[0x2C] = 0x01;

    // done
    return 0;
}

u32 FixNandHeader(const char* path, bool check_size) {
    NandNcsdHeader ncsd;
    if (RebuildNandNcsdHeader(&ncsd) != 0) return 1;

    // safety check
    FILINFO fno;
    FSIZE_t min_size = check_size ? GetNandNcsdMinSizeSectors(&ncsd) * 0x200 : 0x200;
    if ((fvx_stat(path, &fno) != FR_OK) || (min_size > fno.fsize))
        return 1;

    // inject to path
    if (!CheckWritePermissions(path)) return 1;
    return (fvx_qwrite(path, &ncsd, 0x0, 0x200, NULL) == FR_OK) ? 0 : 1;
}

u32 ValidateNandDump(const char* path) {
    NandPartitionInfo info;
    FIL file;

    // truncated path string
    char pathstr[UTF_BUFFER_BYTESIZE(32)];
    TruncateString(pathstr, path, 32, 8);

    // open file
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;

    // check NAND header
    NandNcsdHeader ncsd;
    if ((ReadNandFile(&file, &ncsd, 0, 1, 0xFF) != 0) || (ValidateNandNcsdHeader(&ncsd) != 0)) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_NCSD_HEADER_IS_NOT_VALID);
        fvx_close(&file);
        return 1;
    }

    // check size
    if (fvx_size(&file) < (GetNandNcsdMinSizeSectors(&ncsd) * 0x200)) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_NAND_DUMP_MISSING_DATA);
        fvx_close(&file);
        return 1;
    }

    // check TWL & CTR FAT partitions
    for (u32 i = 0; i < 2; i++) {
        if (i == 0) { // check TWL first, then CTR
            if (GetNandNcsdPartitionInfo(&info, NP_TYPE_STD, NP_SUBTYPE_TWL, 0, &ncsd) != 0) return 1;
        } else if ((GetNandNcsdPartitionInfo(&info, NP_TYPE_STD, NP_SUBTYPE_CTR, 0, &ncsd) != 0) &&
            (GetNandNcsdPartitionInfo(&info, NP_TYPE_STD, NP_SUBTYPE_CTR_N, 0, &ncsd) != 0)) return 1;
        MbrHeader mbr;
        if ((ReadNandFile(&file, &mbr, info.sector, 1, info.keyslot) != 0) ||
            (ValidateMbrHeader(&mbr) != 0)) {
            ShowPrompt(false, "%s\n%s", pathstr, i ? STR_ERROR_CTR_MBR_IS_CORRUPT : STR_ERROR_TWL_MBR_IS_CORRUPT);
            fvx_close(&file);
            return 1; // impossible to happen
        }
        for (u32 p = 0; p < 4; p++) {
            u32 p_sector = mbr.partitions[p].sector;
            u8 fat[0x200];
            if (!p_sector) continue;
            if ((ReadNandFile(&file, fat, info.sector + p_sector, 1, info.keyslot) != 0) ||
                (ValidateFatHeader(fat) != 0)) {
                ShowPrompt(false, i ? STR_PATH_ERROR_CTR_PARTITION_N_IS_CORRUPT : STR_PATH_ERROR_TWL_PARTITION_N_IS_CORRUPT, pathstr, p);
                fvx_close(&file);
                return 1;
            }
        }
    }

    // check FIRMs (at least one FIRM must be valid)
    u8* firm = (u8*) malloc(FIRM_MAX_SIZE);
    if (!firm) return 1;

    // check all 8 firms, also check if ARM9 & ARM11 entrypoints are available
    for (u32 f = 0; f <= 8; f++) {
        if (GetNandNcsdPartitionInfo(&info, NP_TYPE_FIRM, NP_SUBTYPE_CTR, f, &ncsd) != 0) {
            ShowPrompt(false, "%s\n%s", pathstr, STR_NO_VALID_FIRM_FOUND);
            fvx_close(&file);
            free(firm);
            return 1;
        }

        u32 firm_size = info.count * 0x200;
        if ((firm_size <= FIRM_MAX_SIZE) &&
            (ReadNandFile(&file, firm, info.sector, info.count, info.keyslot) == 0) &&
            (ValidateFirm(firm, firm_size, true) == 0))
            break;
    }

    free(firm);
    fvx_close(&file);

    return 0;
}

u32 SafeRestoreNandDump(const char* path) {
    if ((ValidateNandDump(path) != 0) && // NAND dump validation
        !ShowPrompt(true, "%s", STR_ERROR_NAND_DUMP_IS_CORRUPT_STILL_CONTINUE))
        return 1;
    if (!IS_UNLOCKED) {
        ShowPrompt(false, "%s", STR_ERROR_SYSTEM_IS_LOCKED);
        return 1;
    }
    if (fvx_stat("S:/essential.exefs", NULL) != FR_OK) {
        if (ShowPrompt(true, "%s", STR_ESSENTIAL_FILES_BACKUP_IS_REQUIRED_CREATE_ONE_NOW))
            EmbedEssentialBackup("S:/nand.bin");
        else return 1;
    }

    if (!ShowUnlockSequence(5, "%s", STR_WARNING_PROCEEDING_WILL_OVERWRITE_SYSNAND_WITH_DUMP))
        return 1;
    if (!SetWritePermissions(PERM_SYS_LVL1, true)) return 1;

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
    // FIRMS must be at the same place for image and local NAND
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
                if (p >= 1) header_inject = true; // at least one matching FIRM found
                break;
            }
            if ((np_loc != np_img) || (memcmp(&info_loc, &info_img, sizeof(NandPartitionInfo)) != 0))
                break;
        }
        if (!header_inject || (ValidateNandNcsdHeader(&ncsd_img) != 0) || (ValidateMbrHeader(&twl_mbr_img) != 0)) {
            ShowPrompt(false, "%s", STR_IMAGE_NCSD_CORRUPT_OR_CUSTOMIZED_SAFE_RESTORE_NOT_POSSIBLE);
            fvx_close(&file);
            return 1;
        }
    }

    // additional warning for elevated write permissions
    if (header_inject) {
        if (!ShowPrompt(true, "%s", STR_WARNING_NCSD_DIFFERS_BETWEEN_IMAGE_AND_LOCAL_ELEVATED_WRITE_PERMISSIONS_REQUIRED) ||
            !SetWritePermissions(PERM_SYS_LVL3, true)) {
            fvx_close(&file);
            return 1;
        }
    }

    u8* buffer = (u8*) malloc(STD_BUFFER_SIZE);
    if (!buffer) {
        fvx_close(&file);
        return 1;
    }

    // main processing loop
    u32 ret = 0;
    u32 sector0 = SECTOR_SECRET + COUNT_SECRET; // start at the sector after secret sector
    if (!ShowProgress(0, 0, path)) ret = 1;
    for (int p = 0; p < 8; p++) {
        NandPartitionInfo np_info;
        u32 type = NP_TYPE_FIRM;
        u32 subtype = NP_SUBTYPE_CTR;
        u32 sector1 = (GetNandNcsdPartitionInfo(&np_info, type, subtype, p, &ncsd_loc) == 0) ? np_info.sector : fsize / 0x200;
        if (sector1 < sector0) ret = 1; // safety check
        for (u32 s = sector0; (s < sector1) && (ret == 0); s += STD_BUFFER_SIZE / 0x200) {
            u32 count = min(STD_BUFFER_SIZE / 0x200, (sector1 - s));
            if (ReadNandFile(&file, buffer, s, count, 0xFF)) ret = 1;
            if (WriteNandSectors(buffer, s, count, 0xFF, NAND_SYSNAND)) ret = 1;
            if (!ShowProgress(s + count, fsize / 0x200, path)) ret = 1;
        }
        if (sector1 == fsize / 0x200) break; // at file end
        sector0 = np_info.sector + np_info.count; // skip partition
    }

    free(buffer);
    fvx_close(&file);

    // NCSD header inject, should only be required with 2.1 local NANDs on N3DS
    if (header_inject && (ret == 0) &&
        (WriteNandSectors((u8*) &ncsd_img, 0, 1, 0xFF, NAND_SYSNAND) != 0))
        ret = 1;

    return ret;
}

u32 SafeInstallFirmBuffered(const char* path, u32 slots, u8* buffer, u32 bufsiz) {
    char pathstr[UTF_BUFFER_BYTESIZE(32)]; // truncated path string
    TruncateString(pathstr, path, 32, 8);

    // load / check FIRM
    u8* firm = buffer;
    UINT firm_size;
    if ((fvx_qread(path, firm, 0, bufsiz, &firm_size) != FR_OK) ||
        !firm_size || !IsInstallableFirm(firm, firm_size)) {
        ShowPrompt(false, "%s\n%s", pathstr, IsBootableFirm(firm, firm_size) ?
            STR_NOT_AN_INSTALLABLE_FIRM : STR_FIRM_LOAD_VERIFY_ERROR);
        return 1;
    }

    // inject sighax signature, get hash
    u8 firm_sha[0x20];
    memcpy(firm + 0x100, (IS_DEVKIT) ? sig_nand_firm_dev : sig_nand_firm_retail, 0x100);
    sha_quick(firm_sha, firm, firm_size, SHA256_MODE);

    // check install slots
    for (u32 s = 0; s < 8; s++) {
        NandPartitionInfo info;
        if (!((slots>>s)&0x1)) continue;
        if ((GetNandPartitionInfo(&info, NP_TYPE_FIRM, NP_SUBTYPE_CTR, s, NAND_SYSNAND) != 0) ||
            ((info.count * 0x200) < firm_size)) {
            ShowPrompt(false, STR_PATH_FIRM_N_NOT_FOUND_OR_TOO_SMALL, pathstr, s);
            return 1;
        }
    }

    // check sector 0x96 on N3DS, offer fix if required
    u8 sector0x96[0x200] __attribute__((aligned(4)));
    bool fix_sector0x96 = false;
    ReadNandSectors(sector0x96, 0x96, 1, 0x11, NAND_SYSNAND);
    if (!IS_O3DS && !CheckSector0x96Crypto()) {
        ShowPrompt(false, "%s\n%s", pathstr, STR_SECTOR_0X96_CRYPTO_FAIL);
        return 1;
    }
    if (!IS_O3DS && (ValidateSecretSector(sector0x96) != 0)) {
        char path_sector[256];
        strncpy(path_sector, path, 256);
        path_sector[255] = '\0';
        char* slash = strrchr(path_sector, '/');
        if (slash) strncpy(slash+1, "secret_sector.bin", 256 - (slash+1-path_sector));
        else *path_sector = '\0';
        if ((fvx_qread(path_sector, sector0x96, 0, 0x200, NULL) != FR_OK) ||
            (ValidateSecretSector(sector0x96) != 0)) {
            ShowPrompt(false, "%s\n%s", pathstr, STR_SECTOR_0X96_CORRUPTED_PROVIDE_SECRET_SECTOR_BIN_TO_FIX);
            return 1;
        } else if (ShowPrompt(true, "%s\n%s", pathstr, STR_SECTOR_0X96_CORRUPTED_FIX_DURING_INSTALLATION)) {
            fix_sector0x96 = true;
        } else return 1;
    }

    // all checked, ready to go
    if (!ShowUnlockSequence(6, "%s", STR_WARNING_PROCEEDING_WILL_INSTALL_FIRM_TO_SYSNAND_AND_INJECT_SIGHAX_UNSUPPORTED_FIRM_WILL_BRICK)) return 1;
    // if (!SetWritePermissions(PERM_SYS_LVL3, true)) return 1; // one unlock sequence is enough

    // point of no return
    ShowString("%s", STR_INSTALLING_FIRM_PLEASE_WAIT);
    if (fix_sector0x96 && (WriteNandSectors(sector0x96, 0x96, 1, 0x11, NAND_SYSNAND) != 0)) {
        ShowPrompt(false, "%s", STR_THIS_IS_BAD_FAILED_WRITING_SECTOR_0X96_TRY_FIX_BEFORE_REBOOT);
        return 1;
    }
    for (u32 s = 0; s < 8; s++) {
        NandPartitionInfo info;
        if (!((slots>>s)&0x1)) continue;
        if ((GetNandPartitionInfo(&info, NP_TYPE_FIRM, NP_SUBTYPE_CTR, s, NAND_SYSNAND) != 0) ||
            (WriteNandBytes(firm, info.sector*0x200, firm_size, info.keyslot, NAND_SYSNAND) != 0)) {
            ShowPrompt(false, STR_THIS_IS_BAD_FAILED_WRITING_FIRM_N_TRY_FIX_BEFORE_REBOOT, s);
            return 1;
        }
    }

    // done, now check the installation
    ShowString("%s", STR_CHECKING_INSTALLATION_PLEASE_WAIT);
    if (fix_sector0x96 && ((ReadNandSectors(sector0x96, 0x96, 1, 0x11, NAND_SYSNAND) != 0) ||
        (ValidateSecretSector(sector0x96) != 0))) {
        ShowPrompt(false, "%s", STR_THIS_IS_BAD_FAILED_VERIFYING_SECTOR_0X96_TRY_FIX_BEFORE_REBOOT);
        return 1;
    }
    for (u32 s = 0; s < 8; s++) {
        NandPartitionInfo info;
        if (!((slots>>s)&0x1)) continue;
        if ((GetNandPartitionInfo(&info, NP_TYPE_FIRM, NP_SUBTYPE_CTR, s, NAND_SYSNAND) != 0) ||
            (ReadNandBytes(firm, info.sector*0x200, firm_size, info.keyslot, NAND_SYSNAND) != 0) ||
            (sha_cmp(firm_sha, firm, firm_size, SHA256_MODE) != 0)) {
            ShowPrompt(false, STR_THIS_IS_BAD_FAILED_VERIFYING_FIRM_N_TRY_FIX_BEFORE_REBOOT, s);
            return 1;
        }
    }

    return 0;
}

u32 SafeInstallFirm(const char* path, u32 slots) {
    u8* buffer = (u8*) malloc(FIRM_MAX_SIZE);
    if (!buffer) {
        ShowPrompt(false, "%s", STR_OUT_OF_MEMORY);
        return 1;
    }

    u32 ret = SafeInstallFirmBuffered(path, slots, buffer, FIRM_MAX_SIZE);
    free(buffer);
    return ret;
}

u32 SafeInstallKeyDb(const char* path) {
    static const u8 perfect_sha[] = { KEYDB_PERFECT_HASH };
    u8 keydb[KEYDB_PERFECT_SIZE] __attribute__((aligned(4)));

    char pathstr[UTF_BUFFER_BYTESIZE(32)]; // truncated path string
    TruncateString(pathstr, path, 32, 8);

    // already installed?
    if ((ReadNandBytes(keydb, SECTOR_KEYDB*0x200, KEYDB_PERFECT_SIZE, 0xFF, NAND_SYSNAND) == 0) &&
        (sha_cmp(perfect_sha, keydb, KEYDB_PERFECT_SIZE, SHA256_MODE) == 0)) {
        ShowPrompt(false, STR_PERFECT_KEYDB_IS_ALREADY_INSTALLED, KEYDB_NAME);
        return 1;
    }

    // check input path...
    if ((fvx_qread(path, keydb, 0, KEYDB_PERFECT_SIZE, NULL) != FR_OK) ||
        (sha_cmp(perfect_sha, keydb, KEYDB_PERFECT_SIZE, SHA256_MODE) != 0)) {
        ShowPrompt(false, STR_PATH_NOT_PERFECT_KEYDB_IMAGE_CANNOT_INSTALL_TO_NAND, pathstr, KEYDB_NAME);
        return 1;
    }

    // point of no return, install key database
    if (WriteNandBytes(keydb, SECTOR_KEYDB*0x200, KEYDB_PERFECT_SIZE, 0xFF, NAND_SYSNAND) != 0) {
        ShowPrompt(false, STR_PATH_FAILED_WRITING_KEYDB_TO_NAND, pathstr, KEYDB_NAME);
        return 1;
    }

    return 0;
}
