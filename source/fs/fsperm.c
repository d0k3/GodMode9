#include "fsperm.h"
#include "fsdrive.h"
#include "virtual.h"
#include "image.h"
#include "ui.h"

// write permissions - careful with this
static u32 write_permissions = PERM_BASE;

bool CheckWritePermissions(const char* path) {
    char area_name[16];
    int drvtype = DriveType(path);
    u32 perm;
    
    // check mounted image write permissions
    if ((drvtype & DRV_IMAGE) && !CheckWritePermissions(GetMountPath()))
        return false; // endless loop when mounted file inside image, but not possible
    
    // check drive type, get permission type
    if (drvtype & DRV_SYSNAND) {
        perm = PERM_SYSNAND;
        snprintf(area_name, 16, "the SysNAND");
        // check virtual file flags (if any)
        VirtualFile vfile;
        if (GetVirtualFile(&vfile, path) && (vfile.flags & VFLAG_A9LH_AREA)) {
            perm = PERM_A9LH;
            snprintf(area_name, 16, "A9LH regions");
        }
    } else if (drvtype & DRV_EMUNAND) {
        perm = PERM_EMUNAND;
        snprintf(area_name, 16, "the EmuNAND");
    } else if (drvtype & DRV_GAME) {
        perm = PERM_GAME;
        snprintf(area_name, 16, "game images");
    } else if (drvtype & DRV_CART) {
        perm = PERM_CART;
        snprintf(area_name, 16, "gamecarts");
    } else if (drvtype & DRV_XORPAD) {
        perm = PERM_XORPAD;
        snprintf(area_name, 16, "XORpads");
    } else if (drvtype & DRV_IMAGE) {
        perm = PERM_IMAGE;
        snprintf(area_name, 16, "images");
    } else if (drvtype & DRV_MEMORY) {
        perm = PERM_MEMORY;
        snprintf(area_name, 16, "memory areas");
    } else if ((drvtype & DRV_ALIAS) || (strncmp(path, "0:/Nintendo 3DS", 15) == 0)) {
        perm = PERM_SDDATA;
        snprintf(area_name, 16, "SD system data");
    } else if (drvtype & DRV_SDCARD) {
        perm = PERM_SDCARD;
        snprintf(area_name, 16, "the SD card");
    } else if (drvtype & DRV_RAMDRIVE) {
        perm = PERM_RAMDRIVE;
        snprintf(area_name, 16, "the RAM drive");
    } else {
        return false;
    }
    
    // check permission, return if already set
    if ((write_permissions & perm) == perm)
        return true;
    
    // ask the user
    if (!ShowPrompt(true, "Writing to %s is locked!\nUnlock it now?", area_name))
        return false;
        
    return SetWritePermissions(perm, true);
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
        case PERM_RAMDRIVE:
            if (!ShowUnlockSequence(1, "You want to enable RAM drive\nwriting permissions."))
                return false;
        case PERM_EMUNAND:
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND\nwriting permissions."))
                return false;
            break;
        case PERM_IMAGE:
            if (!ShowUnlockSequence(2, "You want to enable image\nwriting permissions."))
                return false;
            break;
        case PERM_GAME:
            ShowPrompt(false, "Unlock write permission for\ngame images is not allowed.");
            return false;
            break;
        case PERM_XORPAD:
            ShowPrompt(false, "Unlock write permission for\nXORpad drive is not allowed.");
            return false;
            break;
        #ifndef SAFEMODE
        case PERM_SYSNAND:
            if (!ShowUnlockSequence(3, "!Better be careful!\n \nYou want to enable SysNAND\nwriting permissions.\nThis enables you to do some\nreally dangerous stuff!"))
                return false;
            break;
        case PERM_A9LH:
            if (!ShowUnlockSequence(5, "!THIS IS YOUR ONLY WARNING!\n \nYou want to enable A9LH area\nwriting permissions.\nThis enables you to OVERWRITE\nyour A9LH installation!"))
                return false;
            break;
        case PERM_MEMORY:
            if (!ShowUnlockSequence(4, "!Better be careful!\n \nYou want to enable memory\nwriting permissions.\nWriting to certain areas may\nlead to unexpected results."))
                return false;
            break;
        case PERM_SDDATA:
            if (!ShowUnlockSequence(2, "You want to enable SD data\nwriting permissions."))
                return false;
            break;
        case PERM_ALL:
            if (!ShowUnlockSequence(3, "!Better be careful!\n \nYou want to enable ALL\nwriting permissions.\nThis enables you to do some\nreally dangerous stuff!"))
                return false;
            break;
        default:
            ShowPrompt(false, "Unlock write permission is not allowed.");
            return false;
            break;
        #else
        case PERM_ALL:
            perm &= ~(PERM_SYSNAND|PERM_MEMORY);
            if (!ShowUnlockSequence(2, "You want to enable EmuNAND &\nimage writing permissions.\nKeep backups, just in case."))
                return false;
            break;
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
