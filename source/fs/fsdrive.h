#pragma once

#include "common.h"
#include "fsdir.h"

#define NORM_FS  10
#define IMGN_FS  3 // image normal filesystems 
#define VIRT_FS  9

// primary drive types
#define DRV_UNKNOWN     (0<<0)
#define DRV_FAT         (1<<0)
#define DRV_VIRTUAL     (1<<1)
// secondary drive types
#define DRV_SDCARD      (1<<2)
#define DRV_SYSNAND     (1<<3)
#define DRV_EMUNAND     (1<<4)
#define DRV_IMAGE       (1<<5)
#define DRV_XORPAD      (1<<6)
#define DRV_RAMDRIVE    (1<<7)
#define DRV_MEMORY      (1<<8)
#define DRV_GAME        (1<<9)
#define DRV_ALIAS       (1<<10)
#define DRV_SEARCH      (1<<11)
#define DRV_STDFAT      (1<<12) // standard FAT drive without limitations

#define FS_DRVNAME \
        "SDCARD", \
        "SYSNAND CTRNAND", "SYSNAND TWLN", "SYSNAND TWLP", \
        "EMUNAND CTRNAND", "EMUNAND TWLN", "EMUNAND TWLP", \
        "IMGNAND CTRNAND", "IMGNAND TWLN", "IMGNAND TWLP", \
        "GAME IMAGE", \
        "SYSNAND SD", "EMUNAND SD", \
        "SYSNAND VIRTUAL", "EMUNAND VIRTUAL", "IMGNAND VIRTUAL", \
        "NAND XORPADS", \
        "MEMORY VIRTUAL", \
        "LAST SEARCH" \
        
#define FS_DRVNUM \
    "0:", "1:", "2:", "3:", "4:", "5:", "6:", "7:", "8:", "9:", "G:", "A:", "B:", "S:", "E:", "I:", "X:", "M:", "Z:"

/** Function to identify the type of a drive **/
int DriveType(const char* path);

/** Set search pattern / path for special Z: drive **/
void SetFSSearch(const char* pattern, const char* path);

/** Get directory content under a given path **/
void GetDirContents(DirStruct* contents, const char* path);

/** Gets remaining space in filesystem in bytes */
uint64_t GetFreeSpace(const char* path);

/** Gets total spacein filesystem in bytes */
uint64_t GetTotalSpace(const char* path);

/** Return the offset - in sectors - of the FAT partition on the drive **/
uint64_t GetPartitionOffsetSector(const char* path);
