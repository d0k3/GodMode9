#pragma once

#include "common.h"

// permission types
#define PERM_SDCARD     (1<<0)
#define PERM_RAMDRIVE   (1<<1)
#define PERM_EMUNAND    (1<<2)
#define PERM_SYSNAND    (1<<3)
#define PERM_IMAGE      (1<<4)
#define PERM_MEMORY     (1<<5)
#define PERM_GAME       (1<<6) // can't be enabled, placeholder
#define PERM_XORPAD     (1<<7) // can't be enabled, placeholder
#define PERM_CART       (1<<8) // can't be enabled, placeholder
#define PERM_A9LH       ((1<<9) | PERM_SYSNAND)
#define PERM_SDDATA     ((1<<10) | PERM_SDCARD)
#define PERM_BASE       (PERM_SDCARD | PERM_RAMDRIVE)
#define PERM_ALL        (PERM_SDCARD | PERM_RAMDRIVE | PERM_EMUNAND | PERM_SYSNAND | PERM_IMAGE | PERM_MEMORY | PERM_SDDATA)

/** Check if writing to this path is allowed **/
bool CheckWritePermissions(const char* path);

/** Set new write permissions */
bool SetWritePermissions(u32 perm, bool add_perm);

/** Get write permissions */
u32 GetWritePermissions();
