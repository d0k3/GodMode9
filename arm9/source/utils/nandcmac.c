#include "nandcmac.h"
#include "fsperm.h"
#include "gba.h"
#include "cmd.h"
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
#define CMAC_CMD_SD        12
#define CMAC_CMD_TWLN      13

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

u32 LocateAgbSaveSdBottomSlot(const char* path, AgbSaveHeader* agbsave) {
    static const u32 save_sizes[] = {
        GBASAVE_EEPROM_512,
        GBASAVE_EEPROM_8K,
        GBASAVE_SRAM_32K,
        GBASAVE_FLASH_64K,
        GBASAVE_FLASH_128K,
        0 };
    AgbSaveHeader hdr;
    u32 offset;

    // search for AGBSAVE bottom slot
    for (u32 i = 0; i < countof(save_sizes); i++) {
        if (save_sizes[i] == 0) return 0; // offset == 0 means no bottom slot found
        offset = sizeof(AgbSaveHeader) + save_sizes[i];
        if (fvx_qread(path, &hdr, offset, sizeof(AgbSaveHeader), NULL) != FR_OK) return 1;
        if (ValidateAgbSaveHeader(&hdr) == 0) break;
    }

    // if valid offset found and pointer given, copy the header
    if (agbsave) memcpy(agbsave, &hdr, sizeof(AgbSaveHeader));
    return offset;
}

u32 LocateAgbSaveSdCurrentSlot(const char* path, AgbSaveHeader* agbsave) {
    AgbSaveHeader hdr_top, hdr_bottom;
    u32 offset_bottom;

    // bottom slot
    offset_bottom = LocateAgbSaveSdBottomSlot(path, &hdr_bottom);
    if (!offset_bottom) return (u32) -1; // doesn't even have a bottom slot, no SD AGB save
    if (agbsave) memcpy(agbsave, &hdr_bottom, sizeof(AgbSaveHeader));

    // top slot
    if ((fvx_qread(path, &hdr_top, 0, sizeof(AgbSaveHeader), NULL) != FR_OK) ||
        (ValidateAgbSaveHeader(&hdr_top) != 0)) return offset_bottom; // no top slot, bottom slot is newer

    // compare slots
    if (hdr_top.times_saved >= hdr_bottom.times_saved) { // top slot is newer or equal
        if (agbsave) memcpy(agbsave, &hdr_top, sizeof(AgbSaveHeader));
        return 0;
    }

    // slots are identical or bottom slot is newer
    return offset_bottom;
}

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

u32 ReadWriteFileCmac(const char* path, u8* cmac, bool do_write, bool check_perms) {
    u32 cmac_type = CalculateFileCmac(path, NULL);
    u32 offset = 0;

    if (!cmac_type) return 1;
    else if (cmac_type == CMAC_MOVABLE) offset = 0x130;
    else if (cmac_type == CMAC_AGBSAVE) offset = 0x010;
    else if (cmac_type == CMAC_AGBSAVE_SD) offset = LocateAgbSaveSdCurrentSlot(path, NULL) + 0x10;
    else if ((cmac_type == CMAC_CMD_SD) || (cmac_type == CMAC_CMD_TWLN)) return 1; // can't do that here
    else offset = 0x000;

    if (do_write && check_perms && !CheckWritePermissions(path)) return 1;
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
            if (CheckCmacHeader(path) == 0) cmac_type = CMAC_SAVEDATA_SD; // Check for 3DS save data first.
            else if (LocateAgbSaveSdBottomSlot(path, NULL) > 0) cmac_type = CMAC_AGBSAVE_SD;
        } else if ((sscanf(path, "%c:/title/%08lx/%08lx/content/cmd/%08lx.cmd", &drv, &tid_high, &tid_low, &sid) == 4) &&
            ext && (strncasecmp(ext, "cmd", 4) == 0)) {
            cmac_type = CMAC_CMD_SD; // this needs special handling, it's in here just for detection
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
    } else if ((drv == '2') || (drv == '5') || (drv == '8')) { // data on TWLN
        if ((sscanf(path, "%c:/title/00030004/%08lx/content/cmd/%08lx.cmd", &drv, &tid_low, &sid) == 3) &&
            ext && (strncasecmp(ext, "cmd", 4) == 0)) {
            cmac_type = CMAC_CMD_TWLN;
        }
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
    else if ((cmac_type == CMAC_CMD_SD) || (cmac_type == CMAC_CMD_TWLN)) return 1;
    else if (!cmac_type) return 1;

    static const u32 cmac_keyslot[] = { CMAC_KEYSLOT };
    u8 hashdata[0x200] __attribute__((aligned(4)));
    u32 keyslot = cmac_keyslot[cmac_type];
    u32 hashsize = 0;

    // setup slot 0x30 via movable.sed
    if ((keyslot == 0x30) && (SetupSlot0x30(drv) != 0))
        return 1;

    // build hash data block, get size
    if ((cmac_type == CMAC_AGBSAVE) || (cmac_type == CMAC_AGBSAVE_SD)) { // agbsaves
        AgbSaveHeader* agbsave = (AgbSaveHeader*) malloc(AGBSAVE_MAX_SIZE);
        u32 offset = 0;
        UINT br;

        if (!agbsave) return 1;
        if (cmac_type == CMAC_AGBSAVE_SD) offset = LocateAgbSaveSdCurrentSlot(path, NULL);
        if ((fvx_qread(path, agbsave, offset, AGBSAVE_MAX_SIZE, &br) != FR_OK) || (br < 0x200) ||
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
    u32 cmac_type = CalculateFileCmac(path, NULL);
    if ((cmac_type == CMAC_CMD_SD) || (cmac_type == CMAC_CMD_TWLN)) {
        return CheckCmdCmac(path);
    } else if (cmac_type) {
        u8 fcmac[16];
        u8 ccmac[16];
        return ((ReadFileCmac(path, fcmac) == 0) && (CalculateFileCmac(path, ccmac) == 0) &&
            (memcmp(fcmac, ccmac, 16) == 0)) ? 0 : 1;
    } else return 1;
}

u32 FixFileCmac(const char* path, bool check_perms) {
    u32 cmac_type = CalculateFileCmac(path, NULL);
    if ((cmac_type == CMAC_CMD_SD) || (cmac_type == CMAC_CMD_TWLN)) {
        return FixCmdCmac(path, check_perms);
    } else if (cmac_type) {
        u8 ccmac[16];
        return ((CalculateFileCmac(path, ccmac) == 0) && (WriteFileCmac(path, ccmac, check_perms) == 0)) ? 0 : 1;
    } else return 1;
}

u32 FixAgbSaveCmac(void* data, u8* cmac, const char* sddrv) {
    AgbSaveHeader* agbsave = (AgbSaveHeader*) (void*) data;
    u8 temp[0x30] __attribute__((aligned(4))); // final hash @temp+0x00

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

u32 CheckFixCmdCmac(const char* path, bool fix, bool check_perms) {
    u8 cmac[16] __attribute__((aligned(4)));
    u32 keyslot = ((*path == 'A') || (*path == 'B')) ? 0x30 : 0x0B;
    bool fixed = false;

    // setup the keyslot if required
    if ((keyslot == 0x30) && (SetupSlot0x30(*path) != 0))
        return 1;

    // set up the temporary path for contents
    u32 pos_name_content = 2 + 1 + 5 + 1 + 8 + 1 + 8 + 1 + 7 + 1;
    char path_content[256]; // that will be more than enough
    char* name_content = path_content + pos_name_content;
    strncpy(path_content, path, 256);
    if (strnlen(path_content, 256) < pos_name_content)
        return 1;

    // hacky check for DLC conents
    bool is_dlc = (strncasecmp(path + 2 + 1 + 5 + 1, "0004008c", 8) == 0);

    // cmd data
    u64 cmd_size = fvx_qsize(path);
    u8* cmd_data = malloc(cmd_size);
    CmdHeader* cmd = (CmdHeader*) (void*) cmd_data;

    // check for out of memory
    if (cmd_data == NULL) return 1;

    // read the full file to memory and check it (we may write it back later)
    if ((fvx_qread(path, cmd_data, 0, cmd_size, NULL) != FR_OK) ||
        (CMD_SIZE(cmd) != cmd_size)) {
        free(cmd_data);
        return 1;
    }

    // we abuse the unknown u32 to mark custom, unfinished CMDs
    bool fix_missing = false;
    if (cmd->unknown == 0xFFFFFFFE) {
        fixed = true;
        cmd->unknown = 0x1;
        fix_missing = true;
    }

    // now, check the CMAC@0x10
    use_aeskey(keyslot);
    aes_cmac(cmd_data, cmac, 1);
    if (memcmp(cmd->cmac, cmac, 0x10) != 0) {
        if (fix) {
            fixed = true;
            memcpy(cmd->cmac, cmac, 0x10);
        } else {
            free(cmd_data);
            return 1;
        }
    }

    // further checking will be more complicated
    // set up pointers to cmd data (pointer arithmetic is hard)
    u32 n_entries = cmd->n_entries;
    u32* cnt_id = (u32*) (cmd + 1);
    u8* cnt_cmac = (u8*) (cnt_id + cmd->n_entries + cmd->n_cmacs);

    // check all ids and cmacs
    for (u32 cnt_idx = 0; cnt_idx < n_entries; cnt_idx++, cnt_id++, cnt_cmac += 0x10) {
        u8 hashdata[0x108] __attribute__((aligned(4)));
        u8 shasum[32];
        if (*cnt_id == 0xFFFFFFFF) continue; // unavailable content
        snprintf(name_content, 32, "%s%08lX.app", (is_dlc) ? "00000000/" : "", *cnt_id);
        if (fvx_qread(path_content, hashdata, 0x100, 0x100, NULL) != FR_OK) {
            if (fix_missing) {
                *cnt_id = 0xFFFFFFFF;
                continue;
            } else {
                free(cmd_data);
                return 1; // failed to read content
            }
        }
        memcpy(hashdata + 0x100, &cnt_idx, 4);
        memcpy(hashdata + 0x104, cnt_id, 4);
        // hash block complete, check it
        sha_quick(shasum, hashdata, 0x108, SHA256_MODE);
        use_aeskey(keyslot);
        aes_cmac(shasum, cmac, 2);
        if (memcmp(cnt_cmac, cmac, 0x10) != 0) {
            if (fix) {
                fixed = true;
                memcpy(cnt_cmac, cmac, 0x10);
            } else {
                free(cmd_data);
                return 1; // bad cmac
            }
        }
    }

    // if fixing is enabled, write back cmd file
    if (fix && fixed && (!check_perms || CheckWritePermissions(path)) &&
        (fvx_qwrite(path, cmd_data, 0, cmd_size, NULL) != FR_OK)) {
        free(cmd_data);
        return 1;
    }

    // if we end up here, everything is fine
    free(cmd_data);
    return 0;
}

u32 RecursiveFixFileCmacWorker(char* path) {
    FILINFO fno;
    DIR pdir;
    u32 err = 0;

    if (fvx_opendir(&pdir, path) == FR_OK) { // process folder contents
        char pathstr[UTF_BUFFER_BYTESIZE(32)];
        TruncateString(pathstr, path, 32, 8);
        char* fname = path + strnlen(path, 255);
        *(fname++) = '/';

        ShowString("%s\n%s", pathstr, STR_FIXING_CMACS_PLEASE_WAIT);
        while (f_readdir(&pdir, &fno) == FR_OK) {
            if ((strncmp(fno.fname, ".", 2) == 0) || (strncmp(fno.fname, "..", 3) == 0))
                continue; // filter out virtual entries
            strncpy(fname, fno.fname, path + 255 - fname);
            if (fno.fname[0] == 0) {
                break;
            } else if (fno.fattrib & AM_DIR) { // directory, recurse through it
                if (RecursiveFixFileCmacWorker(path) != 0) err = 1;
            } else if (CheckCmacPath(path) == 0) { // file, try to fix the CMAC
                if (FixFileCmac(path, true) != 0) err = 1;
                ShowString("%s\n%s", pathstr, STR_FIXING_CMACS_PLEASE_WAIT);
            }
        }
        f_closedir(&pdir);
        *(--fname) = '\0';
    } else if (CheckCmacPath(path) == 0) // fix single file CMAC
        return FixFileCmac(path, true);

    return err;
}

u32 RecursiveFixFileCmac(const char* path) {
    // create a fixed up local path
    // (this is highly path sensitive)
    char lpath[256];
    char* p = (char*) path;
    lpath[255] = '\0';
    for (u32 i = 0; i < 255; i++) {
        lpath[i] = *(p++);
        while ((lpath[i] == '/') && (*p == '/')) p++;
        if (!lpath[i]) {
            if (i && (lpath[i-1] == '/'))
                lpath[i-1] = '\0';
            break;
        }
    }

    return RecursiveFixFileCmacWorker(lpath);
}
