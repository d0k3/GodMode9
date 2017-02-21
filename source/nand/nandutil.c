#include "nandutil.h"
#include "nand.h"
#include "firm.h"
#include "fatmbr.h"
#include "essentials.h" // for essential backup struct
#include "image.h"
#include "fsinit.h"
#include "fsperm.h"
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
    const ExeFsFileHeader filelist[] = {
        { "nand_hdr", 0x000, 0x200 },
        { "secinfo" , 0x200, 0x111 },
        { "movable" , 0x400, 0x140 },
        { "frndseed", 0x600, 0x110 }
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
    
    // mount original file
    InitImgFS(path_bak);
    
    // check sizes
    if ((files[0].size != 0x200) || (files[1].size != 0x111) ||
        ((files[2].size != 0x120) && (files[2].size != 0x140)) || (files[3].size != 0x110))
        return 1;
    
    // calculate hashes
    for (u32 i = 0; i < 4; i++) 
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
        (fvx_qread(path, embedded, 0x200, sizeof(EssentialBackup), &btr) != FR_OK) ||
        (memcmp(embedded, essential, sizeof(EssentialBackup)) != 0))
        return 1;
    return 0;
}

u32 EmbedEssentialBackup(const char* path) {
    EssentialBackup* essential = (EssentialBackup*) TEMP_BUFFER;
    UINT btw;
    // leaving out the write permissions check here, it's okay
    if ((BuildEssentialBackup(path, essential) != 0) ||
        (CheckNandHeader(essential->nand_hdr) == 0) || // 0 -> header not recognized
        (fvx_qwrite(path, essential, 0x200, sizeof(EssentialBackup), &btw) != FR_OK) ||
        (btw != sizeof(EssentialBackup)))
        return 1;
    return 0;
}

u32 ValidateNandDump(const char* path) {
    const u32 mbr_sectors[] = { SECTOR_TWL, SECTOR_CTR };
    const u32 firm_sectors[] = { SECTOR_FIRM0, SECTOR_FIRM1 };
    u8 buffer[0x200];
    FirmHeader firm;
    MbrHeader mbr;
    u32 nand_type;
    FIL file;
    
    // truncated path string
    char pathstr[32 + 1];
    TruncateString(pathstr, path, 32, 8);
    
    // open file
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    
    // check NAND header
    if ((ReadNandFile(&file, buffer, 0, 1, 0xFF) != 0) ||
        ((nand_type = CheckNandHeader(buffer)) == 0)) { // zero means header not recognized
        ShowPrompt(false, "%s\nHeader does not belong to device", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // check size
    if (fvx_size(&file) < ((nand_type == NAND_TYPE_O3DS) ? NAND_MIN_SECTORS_O3DS : NAND_MIN_SECTORS_N3DS)) {
        ShowPrompt(false, "%s\nNAND dump misses data", pathstr);
        fvx_close(&file);
        return 1;
    }
    
    // check MBRs (TWL & CTR)
    for (u32 i = 0; i < sizeof(mbr_sectors) / sizeof(u32); i++) {
        u32 keyslot = (i == 0) ? 0x03 : (nand_type == NAND_TYPE_O3DS) ? 0x04 : 0x05;
        char* section_type = (i) ? "CTR" : "MBR";
        if ((ReadNandFile(&file, &mbr, mbr_sectors[i], 1, keyslot) != 0) ||
            (ValidateMbrHeader(&mbr) != 0)) {
            ShowPrompt(false, "%s\nError: %s MBR is corrupt", pathstr, section_type);
            fvx_close(&file);
            return 1; // impossible to happen
        }
        for (u32 p = 0; p < 4; p++) {
            u32 p_sector = mbr.partitions[p].sector;
            if (!p_sector) continue;
            if ((ReadNandFile(&file, buffer, mbr_sectors[i] + p_sector, 1, keyslot) != 0) ||
                (ValidateFatHeader(buffer) != 0)) {
                ShowPrompt(false, "%s\nError: %s partition%u is corrupt", pathstr, section_type, p);
                fvx_close(&file);
                return 1;
            }
        }
    }
    
    // check FIRMs (FIRM1 must be valid)
    for (u32 i = 0; i < sizeof(firm_sectors) / sizeof(u32); i++) {
        u32 keyslot = 0x06;
        if ((ReadNandFile(&file, &firm, firm_sectors[i], 1, keyslot) != 0) ||
            (ValidateFirmHeader(&firm, 0) != 0) ||
            (getbe32(firm.dec_magic) != 0)) { // decrypted firms are not allowed
            ShowPrompt(false, "%s\nError: FIRM%u header is corrupt", pathstr, i);
            fvx_close(&file);
            return 1;
        }
        // hash verify all available sections
        if (i == 0) continue; // no hash checks for FIRM0 (might be A9LH)
        for (u32 s = 0; s < 4; s++) {
            FirmSectionHeader* section = firm.sections + s;
            u32 sector = firm_sectors[i] + (section->offset / 0x200);
            u32 count = section->size / 0x200;
            if (!count) continue;
            sha_init(SHA256_MODE);
            // relies on sections being aligned to sectors
            for (u32 c = 0; c < count; c += MAIN_BUFFER_SIZE / 0x200) {
                u32 read_sectors = min(MAIN_BUFFER_SIZE / 0x200, (count - c));
                ReadNandFile(&file, MAIN_BUFFER, sector + c, read_sectors, keyslot);
                sha_update(MAIN_BUFFER, read_sectors * 0x200);
            }
            u8 hash[0x20];
            sha_get(hash);
            if (memcmp(hash, section->hash, 0x20) != 0) {
                ShowPrompt(false, "%s\nFIRM%u/%u hash mismatch", pathstr, i, s);
                fvx_close(&file);
                return 1;
            }
        }
    }
    
    return 0;
}

u32 SafeRestoreNandDump(const char* path) {
    u32 safe_sectors[] = { SAFE_SECTORS };
    FIL file;
    
    if ((ValidateNandDump(path) != 0) && // NAND dump validation
        !ShowPrompt(true, "NAND dump corrupt or not from console.\nStill continue?"))
        return 1;
    if (!CheckA9lh()) {
        ShowPrompt(false, "Error: A9LH not detected.");
        return 1;
    }
    if (!SetWritePermissions(PERM_SYS_LVL2, true)) return 1;
    
    // inject essential backup to NAND
    EssentialBackup* essential = (EssentialBackup*) TEMP_BUFFER;
    if (BuildEssentialBackup("1:/nand.bin", essential) == 0) 
        WriteNandSectors((u8*) essential, ESSENTIAL_SECTOR, (sizeof(EssentialBackup) + 0x1FF) / 0x200, 0xFF, NAND_SYSNAND);
    
    // open file, get size
    if (fvx_open(&file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return 1;
    u32 fsize = fvx_size(&file);
    safe_sectors[(sizeof(safe_sectors) / sizeof(u32)) - 1] = fsize / 0x200;
    
    // main processing loop
    u32 ret = 0;
    if (!ShowProgress(0, 0, path)) ret = 1;
    for (u32 p = 0; p < sizeof(safe_sectors) / sizeof(u32); p += 2) {
        u32 sector0 = safe_sectors[p];
        u32 sector1 = safe_sectors[p+1];
        fvx_lseek(&file, sector0 * 0x200);
        for (u32 s = sector0; (s < sector1) && (ret == 0); s += MAIN_BUFFER_SIZE / 0x200) {
            UINT btr;
            u32 count = min(MAIN_BUFFER_SIZE / 0x200, (sector1 - s));
            if (fvx_read(&file, MAIN_BUFFER, count * 0x200, &btr) != FR_OK) ret = 1;
            if (WriteNandSectors(MAIN_BUFFER, s, count, 0xFF, NAND_SYSNAND)) ret = 1;
            if (btr != count * 0x200) ret = 1;
            if (!ShowProgress(s + count, fsize / 0x200, path)) ret = 1;
        }
    }
    fvx_close(&file);
    
    return ret;
}