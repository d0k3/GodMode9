/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "nand.h"
#include "sdmmc.h"

#define TYPE_SDCARD     0
#define TYPE_SYSNAND    1
#define TYPE_EMUNAND    2

#define SUBTYPE_CTRN    0
#define SUBTYPE_CTRN_N  1
#define SUBTYPE_CTRN_NO 2
#define SUBTYPE_TWLN    3
#define SUBTYPE_TWLP    4
#define SUBTYPE_NONE    5

typedef struct {
    BYTE  type;
    BYTE  subtype;
} FATpartition;

typedef struct {
    DWORD offset;
    DWORD size;
    BYTE  keyslot;
} SubtypeDesc;

FATpartition DriveInfo[7] = {
    { TYPE_SDCARD,  SUBTYPE_NONE },     // 0 - SDCARD
    { TYPE_SYSNAND, SUBTYPE_CTRN },     // 1 - SYSNAND CTRNAND
    { TYPE_SYSNAND, SUBTYPE_TWLN },     // 2 - SYSNAND TWLN
    { TYPE_SYSNAND, SUBTYPE_TWLP },     // 3 - SYSNAND TWLP
    { TYPE_EMUNAND, SUBTYPE_CTRN },     // 4 - EMUNAND CTRNAND
    { TYPE_EMUNAND, SUBTYPE_TWLN },     // 5 - EMUNAND TWLN
    { TYPE_EMUNAND, SUBTYPE_TWLP },     // 6 - EMUNAND TWLP
};

SubtypeDesc SubTypes[5] = {
    { 0x05CAE5, 0x179F1B, 0x4 },        // O3DS CTRNAND
    { 0x05CAD7, 0x20E969, 0x5 },        // N3DS CTRNAND
    { 0x05CAD7, 0x20E969, 0x4 },        // N3DS CTRNAND (downgraded)
    { 0x000097, 0x047DA9, 0x3 },        // TWLN
    { 0x04808D, 0x0105B3, 0x3 }         // TWLP
};

static BYTE nand_type_sys = NAND_TYPE_UNK;
static BYTE nand_type_emu = NAND_TYPE_UNK;



/*-----------------------------------------------------------------------*/
/* Get Drive Subtype helper                                                      */
/*-----------------------------------------------------------------------*/

SubtypeDesc* get_subtype_desc(
    __attribute__((unused))
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    BYTE type = DriveInfo[pdrv].type;
    BYTE subtype = DriveInfo[pdrv].subtype;
    
    if (type == TYPE_SDCARD) {
        return NULL;
    } else if (subtype == SUBTYPE_CTRN) {
        BYTE nand_type = (type == TYPE_SYSNAND) ? nand_type_sys : nand_type_emu;
        if (nand_type != NAND_TYPE_O3DS)
            subtype = (nand_type == NAND_TYPE_N3DS) ? SUBTYPE_CTRN_N : SUBTYPE_CTRN_NO;
    }
    
    return &(SubTypes[subtype]);
}



/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	__attribute__((unused))
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	__attribute__((unused))
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    if (pdrv == 0) { // a mounted SD card is the preriquisite for everything else
        sdmmc_sdcard_init();
    } else if (pdrv < 4) {
        nand_type_sys = CheckNandType(false);
    } else if (pdrv < 7) {
        nand_type_emu = CheckNandType(true);
    }
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	__attribute__((unused))
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{   
    BYTE type = DriveInfo[pdrv].type;
    
    if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_readsectors(sector, count, buff)) {
            return RES_PARERR;
        }
    } else {
        SubtypeDesc* subtype = get_subtype_desc(pdrv);
        BYTE keyslot = subtype->keyslot;
        DWORD isector = subtype->offset + sector;
        
        if (ReadNandSectors(buff, isector, count, keyslot, type == TYPE_EMUNAND))
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
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
    BYTE type = DriveInfo[pdrv].type;
    
    if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_writesectors(sector, count, (BYTE *)buff)) {
            return RES_PARERR;
        }
    } else {
        SubtypeDesc* subtype = get_subtype_desc(pdrv);
        BYTE keyslot = subtype->keyslot;
        DWORD isector = subtype->offset + sector;
        
        if (WriteNandSectors(buff, isector, count, keyslot, type == TYPE_EMUNAND))
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
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	__attribute__((unused))
	BYTE cmd,		/* Control code */
	__attribute__((unused))
	void *buff		/* Buffer to send/receive control data */
)
{
    switch (cmd) {
        case GET_SECTOR_SIZE:
            *((DWORD*) buff) = 0x200;
            return RES_OK;
        case GET_SECTOR_COUNT:
            if (DriveInfo[pdrv].type == TYPE_SDCARD) {
                *((DWORD*) buff) = getMMCDevice(1)->total_size;
            } else {
                *((DWORD*) buff) = get_subtype_desc(pdrv)->size;
            }
            return RES_OK;
        case GET_BLOCK_SIZE:
            *((DWORD*) buff) = 0x2000;
            return RES_OK;
        case CTRL_SYNC:
            // nothing to do here - the disk_write function handles that
            return RES_OK;
    }
	return RES_PARERR;
}
#endif
