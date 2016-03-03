/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "platform.h"
#include "nand.h"
#include "sdmmc.h"

#define TYPE_SDCARD     0
#define TYPE_SYSNAND    1
#define TYPE_EMUNAND    2

#define SUBTYPE_CTRN    0
#define SUBTYPE_CTRN_N  1
#define SUBTYPE_TWLN    2
#define SUBTYPE_TWLP    3
#define SUBTYPE_NONE    4

#define SUBTYPE(pd) ((mode_n3ds && (DriveInfo[pd].subtype == SUBTYPE_CTRN)) ? SUBTYPE_CTRN_N : DriveInfo[pd].subtype)

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

SubtypeDesc SubTypes[4] = {
    { 0x05CAE5, 0x179F1B, 0x4 },        // O3DS CTRNAND
    { 0x05CAD7, 0x20E969, 0x5 },        // N3DS CTRNAND
    { 0x000097, 0x047DA9, 0x3 },        // TWLN
    { 0x04808D, 0x0105B3, 0x3 }         // TWLP
};

static bool mode_n3ds = false;


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
    mode_n3ds = (GetUnitPlatform() == PLATFORM_N3DS);
	sdmmc_sdcard_init(); // multiple inits should not be required (also, below)
    InitNandCrypto();
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
        BYTE subtype = SUBTYPE(pdrv);
        BYTE keyslot = SubTypes[subtype].keyslot;
        DWORD isector = SubTypes[subtype].offset + sector;
        
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
        /*BYTE subtype = SUBTYPE(pdrv);
        BYTE keyslot = SubTypes[subtype].keyslot;
        DWORD isector = SubTypes[subtype].offset + sector;
        
        if (WriteNandSectors(buff, isector, count, keyslot, type == TYPE_EMUNAND))
            return RES_PARERR;*/ // stubbed!
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
                *((DWORD*) buff) = SubTypes[SUBTYPE(pdrv)].size;
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
