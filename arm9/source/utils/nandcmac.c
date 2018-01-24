#include "nandcmac.h"
#include "fsperm.h"
#include "gba.h"
#include "sha.h"
#include "aes.h"
#include "vff.h"
#include "ui.h" // for RecursiveFixFileCmac()

// CMAC types, see:
// https://3dbrew.org/wiki/Savegames#AES_CMAC_header
// https://3dbrew.org/wiki/Nand/private/movable.sed
// https://3dbrew.org/wiki/3DS_Virtual_Console#NAND_Savegame
#define CMAC_EXTDATA_SD     1
#define CMAC_EXTDATA_SYS    2
#define CMAC_SAVEDATA_SYS   3
#define CMAC_SAVE_GAMECARD  4
#define CMAC_SAVEGAME       5 // this is not calculated into a CMAC
#define CMAC_SAVEDATA_SD    6
#define CMAC_TITLEDB_SYS    7
#define CMAC_TITLEDB_SD     8
#define CMAC_MOVABLE        9
#define CMAC_AGBSAVE       10
#define CMAC_AGBSAVE_SD    11

// see: https://www.3dbrew.org/wiki/Savegames#AES_CMAC_header
#define CMAC_SAVETYPE NULL, "CTR-EXT0", "CTR-EXT0", "CTR-SYS0", "CTR-NOR0", "CTR-SAV0", "CTR-SIGN", "CTR-9DB0", "CTR-9DB0", NULL, NULL, NULL

// see: http://3dbrew.org/wiki/AES_Registers#Keyslots
#define CMAC_KEYSLOT 0xFF, 0x30 /*0x3A?*/, 0x30, 0x30, 0x33 /*0x19*/, 0xFF, 0x30, 0x0B, 0x30, 0x0B, 0x24, 0x30

// see: https://www.3dbrew.org/wiki/Title_Database
#define SYS_DB_NAMES "ticket.db", "certs.db", "title.db", "import.db", "tmp_t.db", "tmp_i.db"

// CMAC scan paths - just for reference, information used below
// see: https://www.3dbrew.org/wiki/Savegames#AES_CMAC_header
// see: https://www.3dbrew.org/wiki/Title_Database
//  "%c:/extdata/%08lx/%08lx/%08lx/%08lx"                       SD extdata - drv / xid_high / xid_low / fid_low / fid_high
//  "%c:/data/%016llx%016llx/extdata/%08lx/%08lx/%08lx/%08lx"   SYS extdata - drv / id0_high / id0_low / xid_high / xid_low / fid_low / fid_high
//  "%c:/data/%016llx%016llx/extdata/%08lx/%08lx/Quota.dat"     SYS extdata - drv / id0_high / id0_low / xid_high / xid_low
//  "%c:/data/%016llx%016llx/sysdata/%08lx/%08lx                SYS savedata - drv / id0_high / id0_low / fid_low / fid_high
//  "%c:/title/%08lx/%08lx/data/%08lx.sav"                      SD savedata - drv / tid_high / tid_low / sid
//  "%c:/dbs/ticket.db"                                         SYS database (0x0)
//  "%c:/dbs/cert.db"                                           SYS database (0x1)
//  "%c:/dbs/title.db"                                          SYS database (0x2)
//  "%c:/dbs/import.db"                                         SYS database (0x3)
//  "%c:/dbs/tmp_t.db"                                          SYS database (0x4)
//  "%c:/dbs/tmp_i.db"                                          SYS database (0x5)
//  "%c:/private/movable.sed"                                   movable.sed
//  "%c:/agbsave.bin"                                           virtual AGBSAVE file


u32 SetupSlot0x30(char drv) {
    u8 keyy[16] __attribute__((aligned(32)));
    char movable_path[32];
    
    if ((drv == 'A') || (drv == 'S')) drv = '1';
    else if ((drv == 'B') || (drv == 'E')) drv = '4';
    
    snprintf(movable_path, 32, "%c:/private/movable.sed", drv);
    if (fvx_qread(movable_path, keyy, 0x110, 0x10, NULL) != FR_OK) return 1;
    setup_aeskeyY(0x30, keyy);
    use_aeskey(0x30);
    
    return 0;
}

/*u32 CheckAgbSaveHeader(const char* path) {
    AgbSaveHeader agbsave;
    UINT br;
    
    if ((fvx_qread(path, &agbsave, 0, 0x200, &br) != FR_OK) || (br != 0x200))
        return 1;
    
    return ValidateAgbSaveHeader(&agbsave);
}*/

u32 CheckCmacHeader(const char* path) {
    u8 cmac_hdr[0x100];
    UINT br;
    
    if ((fvx_qread(path, cmac_hdr, 0, 0x100, &br) != FR_OK) || (br != 0x100))
        return 1;
    for (u32 i = 0x10; i < 0x100; i++)
        if (cmac_hdr[i] != 0x00) return 1;
    
    return 0;
}

u32 CheckCmacPath(const char* path) {
    return (CalculateFileCmac(path, NULL)) ? 0 : 1;
}

u32 ReadWriteFileCmac(const char* path, u8* cmac, bool do_write) {
    u32 cmac_type = CalculateFileCmac(path, NULL);
    u32 offset = 0;
    
    if (!cmac_type) return 1;
    else if (cmac_type == CMAC_MOVABLE) offset = 0x130;
    else if ((cmac_type == CMAC_AGBSAVE) ||  (cmac_type == CMAC_AGBSAVE_SD)) offset = 0x010;
    else offset = 0x000;
    
    if (do_write && !CheckWritePermissions(path)) return 1;
    if (!do_write) return (fvx_qread(path, cmac, offset, 0x10, NULL) != FR_OK) ? 1 : 0;
    else return (fvx_qwrite(path, cmac, offset, 0x10, NULL) != FR_OK) ? 1 : 0;
}

u32 CalculateFileCmac(const char* path, u8* cmac) {
    u32 cmac_type = 0;
    char drv = *path; // drive letter
    u32 xid_high, xid_low; // extdata ID
    u32 fid_high, fid_low; // extfile ID
    u32 tid_high, tid_low; // title ID
    u32 sid; // save ID / various uses
    char* name;
    char* ext;
    
    name = strrchr(path, '/'); // filename
    if (!name) return 0; // will not happen
    name++;
    ext = strrchr(name, '.'); // extension
    if (ext) ext++;
    
    if ((drv == 'A') || (drv == 'B')) { // data installed on SD
        if (sscanf(path, "%c:/extdata/%08lx/%08lx/%08lx/%08lx", &drv, &xid_high, &xid_low, &fid_high, &fid_low) == 5) {
            sid = 1;
            cmac_type = CMAC_EXTDATA_SD;
        } else if ((sscanf(path, "%c:/title/%08lx/%08lx/data/%08lx.sav", &drv, &tid_high, &tid_low, &sid) == 4) &&
            ext && (strncasecmp(ext, "sav", 4) == 0)) {
            // cmac_type = (CheckCmacHeader(path) == 0) ? CMAC_SAVEDATA_SD : (CheckAgbSaveHeader(path) == 0) ? CMAC_AGBSAVE_SD : 0;
            cmac_type = (CheckCmacHeader(path) == 0) ? CMAC_SAVEDATA_SD : 0;
        }
    } else if ((drv == '1') || (drv == '4') || (drv == '7')) { // data on CTRNAND
        u64 id0_high, id0_low; // ID0
        if (sscanf(path, "%c:/data/%016llx%016llx/extdata/%08lx/%08lx/%08lx/%08lx", &drv, &id0_high, &id0_low, &xid_high, &xid_low, &fid_high, &fid_low) == 7) {
            sid = 1;
            cmac_type = CMAC_EXTDATA_SYS;
        } else if ((sscanf(path, "%c:/data/%016llx%016llx/extdata/%08lx/%08lx/Quota.dat", &drv, &id0_high, &id0_low, &xid_high, &xid_low) == 5) && (strncasecmp(name, "Quota.dat", 10) == 0)) {
            sid = 0;
            fid_low = fid_high = 0;
            cmac_type = CMAC_EXTDATA_SYS;
        } else if (sscanf(path, "%c:/data/%016llx%016llx/sysdata/%08lx/%08lx", &drv, &id0_high, &id0_low, &fid_low, &fid_high) == 5)
            cmac_type = CMAC_SAVEDATA_SYS;
    }
    
    if (!cmac_type) { // path independent stuff
        const char* db_names[] = { SYS_DB_NAMES };
        for (sid = 0; sid < sizeof(db_names) / sizeof(char*); sid++)
            if (strncasecmp(name, db_names[sid], 16) == 0) break;
        if (sid < sizeof(db_names) / sizeof(char*))
            cmac_type = ((drv == 'A') || (drv == 'B')) ? CMAC_TITLEDB_SD : CMAC_TITLEDB_SYS;
        else if (strncasecmp(name, "movable.sed", 16) == 0)
            cmac_type = CMAC_MOVABLE;
        else if (strncasecmp(name, "agbsave.bin", 16) == 0)
            cmac_type = CMAC_AGBSAVE;
    }
    
    // exit with cmac_type if (u8*) cmac is NULL
    // somewhat hacky, but can be used to check if file has a CMAC
    if (!cmac) return cmac_type;
    else if (!cmac_type) return 1;
    
    const u32 cmac_keyslot[] = { CMAC_KEYSLOT };
    u8 hashdata[0x200]; 
    u32 keyslot = cmac_keyslot[cmac_type];
    u32 hashsize = 0;
    
    // setup slot 0x30 via movable.sed
    if ((keyslot == 0x30) && (SetupSlot0x30(drv) != 0))
        return 1;
    
    // build hash data block, get size
    if ((cmac_type == CMAC_AGBSAVE) || (cmac_type == CMAC_AGBSAVE_SD)) { // agbsaves
        AgbSaveHeader* agbsave = (AgbSaveHeader*) malloc(AGBSAVE_MAX_SIZE);
        UINT br;
        
        if (!agbsave) return 1;
        if ((fvx_qread(path, agbsave, 0, AGBSAVE_MAX_SIZE, &br) != FR_OK) || (br < 0x200) ||
            (ValidateAgbSaveHeader(agbsave) != 0) || (0x200 + agbsave->save_size > br)) {
            free(agbsave);
            return 1;
        }
        
        u32 ret = FixAgbSaveCmac(agbsave, cmac, (cmac_type == CMAC_AGBSAVE) ? NULL : path);
        free(agbsave);
        return ret;
    } else if (cmac_type == CMAC_MOVABLE) { // movable.sed
        // see: https://3dbrew.org/wiki/Nand/private/movable.sed
        if (fvx_qread(path, hashdata, 0, 0x140, NULL) != FR_OK)
            return 1;
        hashsize = 0x130;
    } else { // "savegame" CMACs
        // see: https://3dbrew.org/wiki/Savegames
        const char* cmac_savetype[] = { CMAC_SAVETYPE };
        u8 disa[0x100];
        if (fvx_qread(path, disa, 0x100, 0x100, NULL) != FR_OK)
            return 1;
        memcpy(hashdata, cmac_savetype[cmac_type], 8);
        if ((cmac_type == CMAC_EXTDATA_SD) || (cmac_type == CMAC_EXTDATA_SYS)) {
            memcpy(hashdata + 0x08, &xid_low, 4);
            memcpy(hashdata + 0x0C, &xid_high, 4);
            memcpy(hashdata + 0x10, &sid, 4);
            memcpy(hashdata + 0x14, &fid_low, 4);
            memcpy(hashdata + 0x18, &fid_high, 4);
            memcpy(hashdata + 0x1C, disa, 0x100);
            hashsize = 0x11C;
        } else if (cmac_type == CMAC_SAVEDATA_SYS) {
            memcpy(hashdata + 0x08, &fid_low, 4);
            memcpy(hashdata + 0x0C, &fid_high, 4);
            memcpy(hashdata + 0x10, disa, 0x100);
            hashsize = 0x110;
        } else if (cmac_type == CMAC_SAVEDATA_SD) {
            u8* hashdata0 = hashdata + 0x30;
            memcpy(hashdata0 + 0x00, cmac_savetype[CMAC_SAVEGAME], 8);
            memcpy(hashdata0 + 0x08, disa, 0x100);
            memcpy(hashdata + 0x08, &tid_low, 4);
            memcpy(hashdata + 0x0C, &tid_high, 4);
            sha_quick(hashdata + 0x10, hashdata0, 0x108, SHA256_MODE);
            hashsize = 0x30;
        } else if ((cmac_type == CMAC_TITLEDB_SD) || (cmac_type == CMAC_TITLEDB_SYS)) {
            memcpy(hashdata + 0x08, &sid, 4);
            memcpy(hashdata + 0x0C, disa, 0x100);
            hashsize = 0x10C;
        }
    }
        
    // calculate CMAC
    u8 shasum[32];
    if (!hashsize) return 1;
    sha_quick(shasum, hashdata, hashsize, SHA256_MODE);
    use_aeskey(keyslot);
    aes_cmac(shasum, cmac, 2);
    
    return 0;
}

u32 CheckFileCmac(const char* path) {
    u8 fcmac[16];
    u8 ccmac[16];
    return ((ReadFileCmac(path, fcmac) == 0) && (CalculateFileCmac(path, ccmac) == 0) &&
        (memcmp(fcmac, ccmac, 16) == 0)) ? 0 : 1;
}

u32 FixFileCmac(const char* path) {
    u8 ccmac[16];
    return ((CalculateFileCmac(path, ccmac) == 0) && (WriteFileCmac(path, ccmac) == 0)) ? 0 : 1;
}

u32 FixAgbSaveCmac(void* data, u8* cmac, const char* sddrv) {
    AgbSaveHeader* agbsave = (AgbSaveHeader*) (void*) data;
    u8 temp[0x30]; // final hash @temp+0x00
    
    // safety check
    if (ValidateAgbSaveHeader(agbsave) != 0)
        return 1;
    
    if (!sddrv) { // NAND partition mode
        sha_quick(temp + 0x00, (u8*) data + 0x30, (0x200 - 0x30) + agbsave->save_size, SHA256_MODE);
    } else {
        // see: http://3dbrew.org/wiki/3DS_Virtual_Console#NAND_Savegame_on_SD
        // thanks to TuxSH, AuroraWright and Wolfvak for helping me
        // reverse engineering P9 and figuring out AGBSAVE on SD CMACs
        // this won't work on devkits(!!!)
        const char* cmac_savetype[] = { CMAC_SAVETYPE };
        if (SetupSlot0x30(*sddrv) != 0) return 1;
        
        // first hash (hash0 = AGBSAVE_hash)
        sha_quick(temp + 0x08, (u8*) data + 0x30, (0x200 - 0x30) + agbsave->save_size, SHA256_MODE);
        // second hash (hash1 = CTR-SAV0 + hash0)
        memcpy(temp + 0x00, cmac_savetype[CMAC_SAVEGAME], 8);
        sha_quick(temp + 0x10, temp, 0x28, SHA256_MODE);
        // final hash (hash2 = CTR-SIGN + titleID + hash1)
        memcpy(temp + 0x00, cmac_savetype[CMAC_SAVEDATA_SD], 8);
        memcpy(temp + 0x08, &(agbsave->title_id), 8);
        sha_quick(temp + 0x00, temp, 0x30, SHA256_MODE);
    }
    
    use_aeskey((sddrv) ? 0x30 : 0x24);
    aes_cmac(temp, &(agbsave->cmac), 2);
    if (cmac) memcpy(cmac, &(agbsave->cmac), 0x10);
    
    return 0;
}

u32 RecursiveFixFileCmacWorker(char* path) {
    FILINFO fno;
    DIR pdir;
    
    if (fvx_opendir(&pdir, path) == FR_OK) { // process folder contents
        char pathstr[32 + 1];
        TruncateString(pathstr, path, 32, 8);
        char* fname = path + strnlen(path, 255);
        *(fname++) = '/';
        
        ShowString("%s\nFixing CMACs, please wait...", pathstr);
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, path + 255 - fname);
            if (fno.fname[0] == 0) {
                break;
            } else if (fno.fattrib & AM_DIR) { // directory, recurse through it
                if (RecursiveFixFileCmacWorker(path) != 0) return 1;
            } else if (CheckCmacPath(path) == 0) { // file, try to fix the CMAC
                if (FixFileCmac(path) != 0) return 1;
                ShowString("%s\nFixing CMACs, please wait...", pathstr);
            }
        }
        f_closedir(&pdir);
        *(--fname) = '\0';
    } else if (CheckCmacPath(path) == 0) // fix single file CMAC
        return FixFileCmac(path);
    
    return 0;
}

u32 RecursiveFixFileCmac(const char* path) {
    char lpath[256] = { 0 };
    strncpy(lpath, path, 255);
    return RecursiveFixFileCmacWorker(lpath);
}
