/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "image.h"
#include "ramdrive.h"
#include "nand.h"
#include "sdmmc.h"
#include "rtc.h"


#define FREE_MIN_SECTORS 0x2000 // minimum sectors for the free drive to appear (4MB)

#define FPDRV(pdrv) (((pdrv >= 7) && !imgnand_mode) ? pdrv + 3 : pdrv)
#define PART_INFO(pdrv) (DriveInfo + FPDRV(pdrv))
#define PART_TYPE(pdrv) (DriveInfo[FPDRV(pdrv)].type)

#define TYPE_NONE       0
#define TYPE_SYSNAND    NAND_SYSNAND
#define TYPE_EMUNAND    NAND_EMUNAND
#define TYPE_IMGNAND    NAND_IMGNAND
#define TYPE_SDCARD     (1UL<<4)
#define TYPE_IMAGE      (1UL<<5)
#define TYPE_RAMDRV     (1UL<<6)

#define SUBTYPE_CTRN    0
#define SUBTYPE_CTRN_N  1
#define SUBTYPE_CTRN_NO 2
#define SUBTYPE_TWLN    3
#define SUBTYPE_TWLP    4
#define SUBTYPE_FREE    5
#define SUBTYPE_FREE_N  6
#define SUBTYPE_NONE    7

typedef struct {
    BYTE  type;
    BYTE  subtype;
    DWORD offset;
    DWORD size;
    BYTE  keyslot;
} FATpartition;

FATpartition DriveInfo[13] = {
    { TYPE_SDCARD,  SUBTYPE_NONE, 0, 0, 0xFF },     // 0 - SDCARD
    { TYPE_SYSNAND, SUBTYPE_CTRN, 0, 0, 0xFF },     // 1 - SYSNAND CTRNAND
    { TYPE_SYSNAND, SUBTYPE_TWLN, 0, 0, 0xFF },     // 2 - SYSNAND TWLN
    { TYPE_SYSNAND, SUBTYPE_TWLP, 0, 0, 0xFF },     // 3 - SYSNAND TWLP
    { TYPE_EMUNAND, SUBTYPE_CTRN, 0, 0, 0xFF },     // 4 - EMUNAND CTRNAND
    { TYPE_EMUNAND, SUBTYPE_TWLN, 0, 0, 0xFF },     // 5 - EMUNAND TWLN
    { TYPE_EMUNAND, SUBTYPE_TWLP, 0, 0, 0xFF },     // 6 - EMUNAND TWLP
    { TYPE_IMGNAND, SUBTYPE_CTRN, 0, 0, 0xFF },     // 7 - IMGNAND CTRNAND
    { TYPE_IMGNAND, SUBTYPE_TWLN, 0, 0, 0xFF },     // 8 - IMGNAND TWLN
    { TYPE_IMGNAND, SUBTYPE_TWLP, 0, 0, 0xFF },     // 9 - IMGNAND TWLP
    { TYPE_IMAGE,   SUBTYPE_NONE, 0, 0, 0xFF },     // X - IMAGE
    { TYPE_SYSNAND, SUBTYPE_FREE, 0, 0, 0xFF },     // Y - SYSNAND BONUS
    { TYPE_RAMDRV,  SUBTYPE_NONE, 0, 0, 0xFF }      // Z - RAMDRIVE
};

static BYTE imgnand_mode = 0x00; 



/*-----------------------------------------------------------------------*/
/* Get current FAT time                                                      */
/*-----------------------------------------------------------------------*/

DWORD get_fattime( void ) {
    DsTime dstime;
    get_dstime(&dstime);
    DWORD fattime =
        ((DSTIMEGET(&dstime, bcd_s)&0x3F) >> 1 ) |
        ((DSTIMEGET(&dstime, bcd_m)&0x3F) << 5 ) |
        ((DSTIMEGET(&dstime, bcd_h)&0x3F) << 11) |
        ((DSTIMEGET(&dstime, bcd_D)&0x1F) << 16) |
        ((DSTIMEGET(&dstime, bcd_M)&0x0F) << 21) |
        (((DSTIMEGET(&dstime, bcd_Y)+(2000-1980))&0x7F) << 25);
    
    return fattime;
}



/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	__attribute__((unused))
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	__attribute__((unused))
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
    imgnand_mode = (GetMountState() & IMG_NAND) ? 0x01 : 0x00;
    FATpartition* fat_info = PART_INFO(pdrv);
    BYTE type = PART_TYPE(pdrv);
    
    fat_info->offset = fat_info->size = 0;
    fat_info->keyslot = 0xFF;
    
    if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_init() != 0) return STA_NOINIT|STA_NODISK;
        fat_info->size = getMMCDevice(1)->total_size;
    } else if ((type == TYPE_SYSNAND) || (type == TYPE_EMUNAND) || (type == TYPE_IMGNAND)) {
        NandPartitionInfo nprt_info;
        if ((type == TYPE_EMUNAND) && !GetNandSizeSectors(NAND_EMUNAND)) // size check for EmuNAND
            return STA_NOINIT|STA_NODISK;
        if ((fat_info->subtype == SUBTYPE_CTRN) &&
            (GetNandPartitionInfo(&nprt_info, NP_TYPE_STD, NP_SUBTYPE_CTR, 0, type) != 0) &&
            (GetNandPartitionInfo(&nprt_info, NP_TYPE_STD, NP_SUBTYPE_CTR_N, 0, type) != 0)) {
            return STA_NOINIT|STA_NODISK;
        } else if ((fat_info->subtype == SUBTYPE_TWLN) &&
            (GetNandPartitionInfo(&nprt_info, NP_TYPE_FAT, NP_SUBTYPE_TWL, 0, type) != 0)) {
            return STA_NOINIT|STA_NODISK;
        } else if ((fat_info->subtype == SUBTYPE_TWLP) &&
            (GetNandPartitionInfo(&nprt_info, NP_TYPE_FAT, NP_SUBTYPE_TWL, 1, type) != 0)) {
            return STA_NOINIT|STA_NODISK;
        } else if ((fat_info->subtype == SUBTYPE_FREE) &&
            ((GetNandPartitionInfo(&nprt_info, NP_TYPE_BONUS, NP_SUBTYPE_CTR, 0, type) != 0) ||
             (nprt_info.count < FREE_MIN_SECTORS))) {
            return STA_NOINIT|STA_NODISK;
        }
        fat_info->offset = nprt_info.sector;
        fat_info->size = nprt_info.count;
        fat_info->keyslot = nprt_info.keyslot;
    } else if (type == TYPE_IMAGE) {
        if (!(GetMountState() & IMG_FAT)) return STA_NOINIT|STA_NODISK;
        fat_info->size = (GetMountSize() + 0x1FF) / 0x200;
    } else if (type == TYPE_RAMDRV) {
        InitRamDrive();
        fat_info->size = (GetRamDriveSize() + 0x1FF) / 0x200;
    }
    
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	__attribute__((unused))
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{   
    BYTE type = PART_TYPE(pdrv);
    
    if (type == TYPE_NONE) {
        return RES_PARERR;
    } else if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_readsectors(sector, count, buff))
            return RES_PARERR;
    } else if (type == TYPE_IMAGE) {
        if (ReadImageSectors(buff, sector, count))
            return RES_PARERR;
    } else if (type == TYPE_RAMDRV) {
        if (ReadRamDriveSectors(buff, sector, count))
            return RES_PARERR;
    } else {
        FATpartition* fat_info = PART_INFO(pdrv);
        if (ReadNandSectors(buff, fat_info->offset + sector, count, fat_info->keyslot, type))
            return RES_PARERR;
    }

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	__attribute__((unused))
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
    BYTE type = PART_TYPE(pdrv);
    
    if (type == TYPE_NONE) {
        return RES_PARERR;
    } else if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_writesectors(sector, count, (BYTE *)buff))
            return RES_PARERR;
    } else if (type == TYPE_IMAGE) {
        if (WriteImageSectors(buff, sector, count))
            return RES_PARERR;
    } else if (type == TYPE_RAMDRV) {
        if (WriteRamDriveSectors(buff, sector, count))
            return RES_PARERR;
    } else {
        FATpartition* fat_info = PART_INFO(pdrv);
        if (WriteNandSectors(buff, fat_info->offset + sector, count, fat_info->keyslot, type))
            return RES_PARERR; // unstubbed!
    }

	return RES_OK;
}
#endif



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	__attribute__((unused))
	BYTE pdrv,		/* Physical drive number (0..) */
	__attribute__((unused))
	BYTE cmd,		/* Control code */
	__attribute__((unused))
	void *buff		/* Buffer to send/receive control data */
)
{
    BYTE type = PART_TYPE(pdrv);
    FATpartition* fat_info = PART_INFO(pdrv);
    
    switch (cmd) {
        case GET_SECTOR_SIZE:
            *((DWORD*) buff) = 0x200;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *((DWORD*) buff) = fat_info->size;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *((DWORD*) buff) = ((type == TYPE_IMAGE) || (type == TYPE_RAMDRV)) ? 0x1 : 0x2000;
            return RES_OK;
        case CTRL_SYNC:
            if ((type == TYPE_IMAGE) || (type == TYPE_IMGNAND))
                SyncImage();
            // nothing else to do here - sdmmc.c handles the rest
            return RES_OK;
    }
    
	return RES_PARERR;
}
#endif
