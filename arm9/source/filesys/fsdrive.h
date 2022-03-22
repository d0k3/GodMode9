#pragma once

#include "common.h"
#include "fsdir.h"

#define NORM_FS  10
#define IMGN_FS  3 // image normal filesystems

// primary drive types
#define DRV_UNKNOWN     (0<<0)
#define DRV_FAT         (1UL<<0)
#define DRV_VIRTUAL     (1UL<<1)
// secondary drive types
#define DRV_SDCARD      (1UL<<2)
#define DRV_SYSNAND     (1UL<<3)
#define DRV_EMUNAND     (1UL<<4)
#define DRV_CTRNAND     (1UL<<5)
#define DRV_TWLNAND     (1UL<<6)
#define DRV_IMAGE       (1UL<<7)
#define DRV_XORPAD      (1UL<<8)
#define DRV_RAMDRIVE    (1UL<<9)
#define DRV_MEMORY      (1UL<<10)
#define DRV_GAME        (1UL<<11)
#define DRV_CART        (1UL<<12)
#define DRV_VRAM        (1UL<<13)
#define DRV_ALIAS       (1UL<<14)
#define DRV_BONUS       (1UL<<15)
#define DRV_TITLEMAN    (1UL<<16)
#define DRV_SEARCH      (1UL<<17)
#define DRV_STDFAT      (1UL<<18) // standard FAT drive without limitations

#define DRV_LABEL_LEN   (36)

#define FS_DRVNAME \
        STR_LAB_SDCARD, \
        STR_LAB_SYSNAND_CTRNAND, STR_LAB_SYSNAND_TWLN, STR_LAB_SYSNAND_TWLP, STR_LAB_SYSNAND_SD, STR_LAB_SYSNAND_VIRTUAL, \
        STR_LAB_EMUNAND_CTRNAND, STR_LAB_EMUNAND_TWLN, STR_LAB_EMUNAND_TWLP, STR_LAB_EMUNAND_SD, STR_LAB_EMUNAND_VIRTUAL, \
        STR_LAB_IMGNAND_CTRNAND, STR_LAB_IMGNAND_TWLN, STR_LAB_IMGNAND_TWLP, STR_LAB_IMGNAND_VIRTUAL, \
        STR_LAB_GAMECART, \
        STR_LAB_GAME_IMAGE, STR_LAB_AESKEYDB_IMAGE, STR_LAB_BDRI_IMAGE, STR_LAB_DISA_DIFF_IMAGE, \
        STR_LAB_MEMORY_VIRTUAL, \
        STR_LAB_VRAM_VIRTUAL, \
        STR_LAB_TITLE_MANAGER, \
        STR_LAB_LAST_SEARCH

#define FS_DRVNUM \
    "0:", "1:", "2:", "3:", "A:", "S:", "4:", "5:", "6:", "B:", "E:", "7:", "8:", "9:", \
    "I:", "C:", "G:", "K:", "T:", "D:", "M:", "V:", "Y:", "Z:"

/** Function to identify the type of a drive **/
int DriveType(const char* path);

/** Set search pattern / path / mode for special Z: drive **/
void SetFSSearch(const char* pattern, const char* path);

/** Enable title manager for special processing of mounted title.db **/
void SetTitleManagerMode(bool mode);

/** Read the FAT volume label of a partition **/
bool GetFATVolumeLabel(const char* drv, char* label);

/** Get directory content under a given path **/
void GetDirContents(DirStruct* contents, const char* path);

/** Gets remaining space in filesystem in bytes */
uint64_t GetFreeSpace(const char* path);

/** Gets total spacein filesystem in bytes */
uint64_t GetTotalSpace(const char* path);

/** Return the offset - in sectors - of the FAT partition on the drive **/
uint64_t GetPartitionOffsetSector(const char* path);
