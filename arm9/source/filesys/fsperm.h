#pragma once

#include "common.h"

// permission types
#define PERM_SDCARD     (1UL<<0)
#define PERM_IMAGE      (1UL<<1)
#define PERM_RAMDRIVE   (1UL<<2)
#define PERM_EMU_LVL0   (1UL<<3)
#define PERM_EMU_LVL1   (PERM_EMU_LVL0|(1UL<<4))
#define PERM_SYS_LVL0   (1UL<<5)
#define PERM_SYS_LVL1   (PERM_SYS_LVL0|(1UL<<6))
#define PERM_SYS_LVL2   (PERM_SYS_LVL1|(1UL<<7))
#define PERM_SYS_LVL3   (PERM_SYS_LVL2|(1UL<<8))
#define PERM_SDDATA     (PERM_SDCARD|(1UL<<9))
#define PERM_MEMORY     (1UL<<10)
#define PERM_GAME       (1UL<<11) // can't be enabled, placeholder
#define PERM_XORPAD     (1UL<<12) // can't be enabled, placeholder
#define PERM_CART       (1UL<<13) // can't be enabled, placeholder
#define PERM_VRAM       (1UL<<14) // can't be enabled, placeholder
#define PERM_BASE       (PERM_SDCARD | PERM_IMAGE | PERM_RAMDRIVE | PERM_EMU_LVL0 | PERM_SYS_LVL0)

// permission levels / colors
#define PERM_BLUE       (GetWritePermissions()&PERM_MEMORY)
#define PERM_RED        (GetWritePermissions()&(PERM_SYS_LVL3&~PERM_SYS_LVL2))
#define PERM_ORANGE     (GetWritePermissions()&(PERM_SYS_LVL2&~PERM_SYS_LVL1))
#define PERM_YELLOW     (GetWritePermissions()&((PERM_SYS_LVL1&~PERM_SYS_LVL0)|(PERM_EMU_LVL1&~PERM_EMU_LVL0)|(PERM_SDDATA&~PERM_SDCARD)))
#define PERM_GREEN      (GetWritePermissions()&(PERM_SDCARD|PERM_IMAGE|PERM_RAMDRIVE|PERM_EMU_LVL0|PERM_SYS_LVL0))

/** Check if writing to this path is allowed **/
bool CheckWritePermissions(const char* path);

/** Same as above, but for all containing objects **/
bool CheckDirWritePermissions(const char* path);

/** Set new write permissions */
bool SetWritePermissions(u32 perm, bool add_perm);

/** Get write permissions */
u32 GetWritePermissions();
