#include "fsperm.h"
#include "fsdrive.h"
#include "virtual.h"
#include "image.h"
#include "unittype.h"
#include "ui.h"
#include "sdmmc.h"

#define PATH_SYS_LVL1   "S:/twln.bin", "S:/twlp.bin" 
#define PATH_SYS_LVL2   "1:/rw/sys/LocalFriendCodeSeed_B", "1:/rw/sys/LocalFriendCodeSeed_A", \
                        "1:/rw/sys/SecureInfo_A", "1:/rw/sys/SecureInfo_B", \
                        "1:/private/movable.sed", "1:/ro/sys/HWCAL0.dat", "1:/ro/sys/HWCAL1.dat", \
                        "S:/ctrnand_fat.bin", "S:/ctrnand_full.bin"
#define PATH_SYS_LVL3   "S:/firm0.bin", "S:/firm1.bin", "S:/nand.bin", "S:/nand_minsize.bin", "S:/nand_hdr.bin", \
                        "S:/sector0x96.bin", "S:/twlmbr.bin"
#define PATH_EMU_LVL1   "E:/ctrnand_fat.bin", "E:/ctrnand_full.bin", "E:/nand.bin", "E:/nand_minsize.bin", "E:/nand_hdr.bin"

// write permissions - careful with this
static u32 write_permissions = PERM_BASE;

bool CheckWritePermissions(const char* path) {
    char area_name[16];
    int drvtype = DriveType(path);
    u32 perm;
    
    // check mounted image write permissions
    if ((drvtype & DRV_IMAGE) && !CheckWritePermissions(GetMountPath()))
        return false; // endless loop when mounted file inside image, but not possible
    
    // SD card write protection check
    if ((drvtype & (DRV_SDCARD | DRV_EMUNAND | DRV_ALIAS)) && SD_WRITE_PROTECTED) {
        ShowPrompt(false, "SD card is write protected!\nCan't continue.");
        return false;
    }
    
    // check drive type, get permission type
    if (drvtype & DRV_SYSNAND) {
        u32 perms[] = { PERM_SYS_LVL0, PERM_SYS_LVL1, PERM_SYS_LVL2, PERM_SYS_LVL3 };
        u32 lvl = (drvtype & (DRV_TWLNAND|DRV_ALIAS|DRV_CTRNAND)) ? 1 : 0;
        if (drvtype & (DRV_CTRNAND|DRV_VIRTUAL)) { // check for paths
            const char* path_lvl3[] = { PATH_SYS_LVL3 };
            const char* path_lvl2[] = { PATH_SYS_LVL2 };
            const char* path_lvl1[] = { PATH_SYS_LVL1 };
            for (u32 i = 0; (i < sizeof(path_lvl3) / sizeof(char*)) && (lvl < 3); i++)
                if (strncasecmp(path, path_lvl3[i], 256) == 0) lvl = 3;
            for (u32 i = 0; (i < sizeof(path_lvl2) / sizeof(char*)) && (lvl < 2); i++)
                if (strncasecmp(path, path_lvl2[i], 256) == 0) lvl = 2;
            for (u32 i = 0; (i < sizeof(path_lvl1) / sizeof(char*)) && (lvl < 1); i++)
                if (strncasecmp(path, path_lvl1[i], 256) == 0) lvl = 1;
        }
        if (!IS_A9LH) { // changed SysNAND permission levels on non-A9LH
            if ((drvtype & DRV_CTRNAND) || (lvl == 2)) lvl = 3;
        }
        perm = perms[lvl];
        snprintf(area_name, 16, "SysNAND (lvl%lu)", lvl);
    } else if (drvtype & DRV_EMUNAND) {
        u32 perms[] = { PERM_EMU_LVL0, PERM_EMU_LVL1 };
        u32 lvl = (drvtype & (DRV_ALIAS|DRV_CTRNAND)) ? 1 : 0;
        if (drvtype & DRV_VIRTUAL) { // check for paths
            const char* path_lvl1[] = { PATH_EMU_LVL1 };
            for (u32 i = 0; (i < sizeof(path_lvl1) / sizeof(char*)) && (lvl < 1); i++)
                if (strncasecmp(path, path_lvl1[i], 256) == 0) lvl = 1;
        }
        perm = perms[lvl];
        snprintf(area_name, 16, "EmuNAND (lvl%lu)", lvl);
    } else if (drvtype & DRV_GAME) {
        perm = PERM_GAME;
        snprintf(area_name, 16, "game images");
    } else if (drvtype & DRV_CART) {
        perm = PERM_CART;
        snprintf(area_name, 16, "gamecarts");
    } else if (drvtype & DRV_VRAM) {
        perm = PERM_VRAM;
        snprintf(area_name, 16, "vram0");
    } else if (drvtype & DRV_XORPAD) {
        perm = PERM_XORPAD;
        snprintf(area_name, 16, "XORpads");
    } else if (drvtype & DRV_IMAGE) {
        perm = PERM_IMAGE;
        snprintf(area_name, 16, "images");
    } else if (drvtype & DRV_MEMORY) {
        perm = PERM_MEMORY;
        snprintf(area_name, 16, "memory areas");
    } else if (strncasecmp(path, "0:/Nintendo 3DS", 15) == 0) { // this check could be better
        perm = PERM_SDDATA;
        snprintf(area_name, 16, "SD system data");
    } else if (drvtype & DRV_SDCARD) {
        perm = PERM_SDCARD;
        snprintf(area_name, 16, "SD card");
    } else if (drvtype & DRV_RAMDRIVE) {
        perm = PERM_RAMDRIVE;
        snprintf(area_name, 16, "RAM drive");
    } else {
        return false;
    }
    
    // check permission, return if already set
    if ((write_permissions & perm) == perm)
        return true;
    
    // offer unlock if possible
    if (!(perm & (PERM_VRAM|PERM_CART|PERM_GAME|PERM_XORPAD))) {
        // ask the user
        if (!ShowPrompt(true, "Writing to %s is locked!\nUnlock it now?", area_name))
            return false;
            
        return SetWritePermissions(perm, true);
    }
    
    // unlock not possible
    ShowPrompt(false, "Unlock write permission for\n%s is not allowed.", area_name);
    return false;
}

bool CheckDirWritePermissions(const char* path) {
    const char* path_chk[] = { PATH_SYS_LVL3, PATH_SYS_LVL2, PATH_SYS_LVL1, PATH_EMU_LVL1 };
    for (u32 i = 0; i < sizeof(path_chk) / sizeof(char*); i++) {
        const char* path_cmp = path_chk[i];
        u32 p = 0;
        for (; p < 256; p++)
            if (!path[p] || !path_cmp[p] || (path[p] != path_cmp[p])) break;
        if (!path[p] && (path_cmp[p] == '/'))
            return CheckWritePermissions(path_cmp); // special dir, check object
    }
    return CheckWritePermissions(path); // not a special dir, just check path
}

bool SetWritePermissions(u32 perm, bool add_perm) {
    if ((write_permissions & perm) == perm) { // write permissions already given
        if (!add_perm) write_permissions = perm;
        return true;
    }
    
    switch (perm) {
        case PERM_BASE:
            if (!ShowUnlockSequence(1, "You want to enable base\nwriting permissions."))
                return false;
            break;
        case PERM_SDCARD:
            if (!ShowUnlockSequence(1, "You want to enable SD card\nwriting permissions."))
                return false;
            break;
        case PERM_IMAGE:
            if (!ShowUnlockSequence(1, "You want to enable image\nwriting permissions."))
                return false;
            break;
        case PERM_RAMDRIVE:
            if (!ShowUnlockSequence(1, "You want to enable RAM drive\nwriting permissions."))
                return false;
            break;
        case PERM_EMU_LVL0:
            if (!ShowUnlockSequence(1, "You want to enable EmuNAND\nlvl0 writing permissions."))
                return false;
            break;
        case PERM_SYS_LVL0:
            if (!ShowUnlockSequence(1, "You want to enable SysNAND\nlvl0 writing permissions."))
                return false;
            break;
        case PERM_EMU_LVL1:
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND\nlvl1 writing permissions.\n \nThis enables you to modify\nrecoverable system data,\nuser data & savegames."))
                return false;
            break;
        case PERM_SYS_LVL1:
            if (!ShowUnlockSequence(2, "You want to enable SysNAND\nlvl1 writing permissions.\n \nThis enables you to modify\nsystem data, installations,\nuser data & savegames."))
                return false;
            break;
        case PERM_SDDATA:
            if (!ShowUnlockSequence(2, "You want to enable SD data\nwriting permissions.\n \nThis enables you to modify\ninstallations, user data &\nsavegames."))
                return false;
            break;
        #ifndef SAFEMODE
        case PERM_SYS_LVL2:
            if (!ShowUnlockSequence(3, "!Better be careful!\n \nYou want to enable SysNAND\nlvl2 writing permissions.\n \nThis enables you to modify\nirrecoverable system data!"))
                return false;
            break;
        case PERM_MEMORY:
            if (!ShowUnlockSequence(4, "!Better be careful!\n \nYou want to enable memory\nwriting permissions.\n \nWriting to certain areas may\nlead to unexpected results."))
                return false;
            break;
        case PERM_SYS_LVL3:
            if (!ShowUnlockSequence(6, "!THIS IS YOUR ONLY WARNING!\n \nYou want to enable SysNAND\nlvl3 writing permissions.\n \nThis enables you to OVERWRITE\n%s", IS_SIGHAX ? "your B9S installation and/or\nBRICK your console!" : IS_A9LH ? "your A9LH installation and/or\nBRICK your console!" : "essential system files and/or\nBRICK your console!"))
                return false;
            break;
        default:
            ShowPrompt(false, "Unlock write permission is not allowed.");
            return false;
            break;
        #else
        default:
            ShowPrompt(false, "Can't unlock write permission.\nTry GodMode9 instead!");
            return false;
            break;
        #endif
    }
    
    write_permissions = add_perm ? write_permissions | perm : perm;
    
    return true;
}

u32 GetWritePermissions() {
    return write_permissions;
}
