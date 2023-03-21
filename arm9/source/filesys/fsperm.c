#include "fsperm.h"
#include "fsdrive.h"
#include "virtual.h"
#include "image.h"
#include "unittype.h"
#include "essentials.h"
#include "language.h"
#include "ui.h"
#include "sdmmc.h"

#define PATH_SYS_LVL1   "S:/twln.bin", "S:/twlp.bin"
#define PATH_SYS_LVL2   "1:/rw/sys/LocalFriendCodeSeed_B", "1:/rw/sys/LocalFriendCodeSeed_A", \
                        "1:/rw/sys/SecureInfo_A", "1:/rw/sys/SecureInfo_B", \
                        "1:/private/movable.sed", "1:/ro/sys/HWCAL0.dat", "1:/ro/sys/HWCAL1.dat", \
                        "S:/ctrnand_fat.bin", "S:/ctrnand_full.bin", "S:/" ESSENTIAL_NAME
#define PATH_SYS_LVL3   "S:/firm0.bin", "S:/firm1.bin", "S:/nand.bin", "S:/nand_minsize.bin", "S:/nand_hdr.bin", \
                        "S:/sector0x96.bin", "S:/twlmbr.bin"
#define PATH_EMU_LVL1   "E:/ctrnand_fat.bin", "E:/ctrnand_full.bin", "E:/nand.bin", "E:/nand_minsize.bin", "E:/nand_hdr.bin"

// write permissions - careful with this
static u32 write_permissions = PERM_BASE;

bool CheckWritePermissions(const char* path) {
    char area_name[UTF_BUFFER_BYTESIZE(16)];
    int drvtype = DriveType(path);
    u32 perm;

    // create a standardized path string
    char path_f[256];
    char* p = (char*) path;
    path_f[255] = '\0';
    for (u32 i = 0; i < 255; i++) {
        path_f[i] = *(p++);
        while ((path_f[i] == '/') && (*p == '/')) p++;
        if (!path_f[i]) break;
    }

    // check mounted image write permissions
    if ((drvtype & DRV_IMAGE) && !CheckWritePermissions(GetMountPath()))
        return false; // endless loop when mounted file inside image, but not possible

    // SD card write protection check
    if ((drvtype & (DRV_SDCARD | DRV_EMUNAND | DRV_ALIAS)) && SD_WRITE_PROTECTED) {
        ShowPrompt(false, "%s", STR_SD_WRITE_PROTECTED_CANT_CONTINUE);
        return false;
    }

    // check drive type, get permission type
    if (drvtype & DRV_SYSNAND) {
        static const u32 perms[] = { PERM_SYS_LVL0, PERM_SYS_LVL1, PERM_SYS_LVL2, PERM_SYS_LVL3 };
        u32 lvl = (drvtype & (DRV_TWLNAND|DRV_ALIAS|DRV_CTRNAND)) ? 1 : 0;
        if (drvtype & (DRV_CTRNAND|DRV_VIRTUAL)) { // check for paths
            const char* path_lvl3[] = { PATH_SYS_LVL3 };
            const char* path_lvl2[] = { PATH_SYS_LVL2 };
            const char* path_lvl1[] = { PATH_SYS_LVL1 };
            for (u32 i = 0; (i < sizeof(path_lvl3) / sizeof(char*)) && (lvl < 3); i++)
                if (strncasecmp(path_f, path_lvl3[i], 256) == 0) lvl = 3;
            for (u32 i = 0; (i < sizeof(path_lvl2) / sizeof(char*)) && (lvl < 2); i++)
                if (strncasecmp(path_f, path_lvl2[i], 256) == 0) lvl = 2;
            for (u32 i = 0; (i < sizeof(path_lvl1) / sizeof(char*)) && (lvl < 1); i++)
                if (strncasecmp(path_f, path_lvl1[i], 256) == 0) lvl = 1;
        }
        if (!IS_UNLOCKED) { // changed SysNAND permission levels on locked systems
            if ((drvtype & DRV_CTRNAND) || (lvl == 2)) lvl = 3;
        }
        perm = perms[lvl];
        snprintf(area_name, sizeof(area_name), STR_SYSNAND_LVL_N, lvl);
    } else if (drvtype & DRV_EMUNAND) {
        static const u32 perms[] = { PERM_EMU_LVL0, PERM_EMU_LVL1 };
        u32 lvl = (drvtype & (DRV_ALIAS|DRV_CTRNAND)) ? 1 : 0;
        if (drvtype & DRV_VIRTUAL) { // check for paths
            const char* path_lvl1[] = { PATH_EMU_LVL1 };
            for (u32 i = 0; (i < sizeof(path_lvl1) / sizeof(char*)) && (lvl < 1); i++)
                if (strncasecmp(path_f, path_lvl1[i], 256) == 0) lvl = 1;
        }
        perm = perms[lvl];
        snprintf(area_name, sizeof(area_name), STR_EMUNAND_LVL_N, lvl);
    } else if (drvtype & DRV_GAME) {
        perm = PERM_GAME;
        snprintf(area_name, sizeof(area_name), "%s", STR_GAME_IMAGES);
    } else if (drvtype & DRV_CART) {
        perm = PERM_CART;
        snprintf(area_name, sizeof(area_name), "%s", STR_GAMECART_SAVES);
    } else if (drvtype & DRV_VRAM) {
        perm = PERM_VRAM;
        snprintf(area_name, sizeof(area_name), "vram0");
    } else if (drvtype & DRV_XORPAD) {
        perm = PERM_XORPAD;
        snprintf(area_name, sizeof(area_name), "XORpads");
    } else if (drvtype & DRV_IMAGE) {
        perm = PERM_IMAGE;
        snprintf(area_name, sizeof(area_name), "%s", STR_IMAGES);
    } else if (drvtype & DRV_MEMORY) {
        perm = PERM_MEMORY;
        snprintf(area_name, sizeof(area_name), "%s", STR_MEMORY_AREAS);
    } else if (strncasecmp(path_f, "0:/Nintendo 3DS", 15) == 0) { // this check could be better
        perm = PERM_SDDATA;
        snprintf(area_name, sizeof(area_name), "%s", STR_SD_SYSTEM_DATA);
    } else if (drvtype & DRV_SDCARD) {
        perm = PERM_SDCARD;
        snprintf(area_name, sizeof(area_name), "%s", STR_SD_CARD);
    } else if (drvtype & DRV_RAMDRIVE) {
        perm = PERM_RAMDRIVE;
        snprintf(area_name, sizeof(area_name), "%s", STR_RAM_DRIVE);
    } else {
        return false;
    }

    // check permission, return if already set
    if ((write_permissions & perm) == perm)
        return true;

    // offer unlock if possible
    if (!(perm & (PERM_VRAM|PERM_GAME|PERM_XORPAD))) {
        // ask the user
        if (!ShowPrompt(true, STR_WRITING_TO_DRIVE_IS_LOCKED_UNLOCK_NOW, area_name))
            return false;

        return SetWritePermissions(perm, true);
    }

    // unlock not possible
    ShowPrompt(false, STR_UNLOCK_WRITE_FOR_DRIVE_NOT_ALLOWED, area_name);
    return false;
}

bool CheckDirWritePermissions(const char* path) {
    static const char* path_chk[] = { PATH_SYS_LVL3, PATH_SYS_LVL2, PATH_SYS_LVL1, PATH_EMU_LVL1 };
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
            if (!ShowUnlockSequence(1, "%s", STR_ENABLE_BASE_WRITE))
                return false;
            break;
        case PERM_SDCARD:
            if (!ShowUnlockSequence(1, "%s", STR_ENABLE_SD_WRITE))
                return false;
            break;
        case PERM_IMAGE:
            if (!ShowUnlockSequence(1, "%s", STR_ENABLE_IMAGE_WRITE))
                return false;
            break;
        case PERM_RAMDRIVE:
            if (!ShowUnlockSequence(1, "%s", STR_ENABLE_RAM_DRIVE_WRITE))
                return false;
            break;
        case PERM_EMU_LVL0:
            if (!ShowUnlockSequence(1, "%s", STR_ENABLE_EMUNAND_0_WRITE))
                return false;
            break;
        case PERM_SYS_LVL0:
            if (!ShowUnlockSequence(1, "%s", STR_ENABLE_SYSNAND_0_WRITE))
                return false;
            break;
        case PERM_EMU_LVL1:
            if (!ShowUnlockSequence(2, "%s", STR_ENABLE_EMUNAND_1_WRITE))
                return false;
            break;
        case PERM_SYS_LVL1:
            if (!ShowUnlockSequence(2, "%s", STR_ENABLE_SYSNAND_1_WRITE))
                return false;
            break;
        case PERM_CART:
            if (!ShowUnlockSequence(2, "%s", STR_ENABLE_GAMECART_SAVE_WRITE))
                return false;
            break;
        #ifndef SAFEMODE
        case PERM_SYS_LVL2:
            if (!ShowUnlockSequence(3, "%s", STR_ENABLE_SYSNAND_2_WRITE))
                return false;
            break;
        case PERM_MEMORY:
            if (!ShowUnlockSequence(4, "%s", STR_ENABLE_MEMORY_WRITE))
                return false;
            break;
        case PERM_SDDATA:
            if (!ShowUnlockSequence(5, "%s", STR_ENABLE_SD_DATA_WRITE))
                return false;
            break;
        case PERM_SYS_LVL3:
            if (!ShowUnlockSequence(6, "%s", STR_ENABLE_SYSNAND_3_WRITE))
                return false;
            break;
        default:
            ShowPrompt(false, "%s", STR_UNLOCK_WRITE_NOT_ALLOWED);
            return false;
            break;
        #else
        default:
            ShowPrompt(false, "%s", STR_CANT_UNLOCK_WRITE_TRY_GODMODE9);
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
