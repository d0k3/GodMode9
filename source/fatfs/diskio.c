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
#include "sdmmc.h"
#include "decryptor/nand.h"

#define TYPE_SDCARD 0
#define TYPE_SYSNAND 1
#define TYPE_EMUNAND 2

typedef struct {
    DWORD offset;
    DWORD subtype;
    BYTE type;
} FATpartition;

FATpartition DriveInfo[28] = {
    { 0x000000, TYPE_SCARD, 0 },            //  0 - SDCARD
    { 0x000000, TYPE_SYSNAND, P_CTRNAND },  //  1 - SYSNAND CTRNAND
    { 0x000000, TYPE_SYSNAND, P_TWLN },     //  2 - SYSNAND TWLN
    { 0x000000, TYPE_SYSNAND, P_TWLP },     //  3 - SYSNAND TWLP
    { 0x000000, TYPE_EMUNAND, P_CTRNAND },  //  4 - EMUNAND0 O3DS CTRNAND
    { 0x000000, TYPE_EMUNAND, P_TWLN },     //  5 - EMUNAND0 O3DS TWLN
    { 0x000000, TYPE_EMUNAND, P_TWLP },     //  6 - EMUNAND0 O3DS TWLP
    { 0x200000, TYPE_EMUNAND, P_CTRNAND },  //  7 - EMUNAND1 O3DS CTRNAND
    { 0x200000, TYPE_EMUNAND, P_TWLN },     //  8 - EMUNAND1 O3DS TWLN
    { 0x200000, TYPE_EMUNAND, P_TWLP },     //  9 - EMUNAND1 O3DS TWLP
    { 0x400000, TYPE_EMUNAND, P_CTRNAND },  // 10 - EMUNAND2 O3DS CTRNAND
    { 0x400000, TYPE_EMUNAND, P_TWLN },     // 11 - EMUNAND2 O3DS TWLN
    { 0x400000, TYPE_EMUNAND, P_TWLP },     // 12 - EMUNAND2 O3DS TWLP
    { 0x600000, TYPE_EMUNAND, P_CTRNAND },  // 13 - EMUNAND3 O3DS CTRNAND
    { 0x600000, TYPE_EMUNAND, P_TWLN },     // 14 - EMUNAND3 O3DS TWLN
    { 0x600000, TYPE_EMUNAND, P_TWLP },     // 15 - EMUNAND3 O3DS TWLP
    { 0x000000, TYPE_EMUNAND, P_CTRNAND },  // 16 - EMUNAND0 N3DS CTRNAND
    { 0x000000, TYPE_EMUNAND, P_TWLN },     // 17 - EMUNAND0 N3DS TWLN
    { 0x000000, TYPE_EMUNAND, P_TWLP },     // 18 - EMUNAND0 N3DS TWLP
    { 0x400000, TYPE_EMUNAND, P_CTRNAND },  // 19 - EMUNAND1 N3DS CTRNAND
    { 0x400000, TYPE_EMUNAND, P_TWLN },     // 20 - EMUNAND1 N3DS TWLN
    { 0x400000, TYPE_EMUNAND, P_TWLP },     // 21 - EMUNAND1 N3DS TWLP
    { 0x800000, TYPE_EMUNAND, P_CTRNAND },  // 22 - EMUNAND2 N3DS CTRNAND
    { 0x800000, TYPE_EMUNAND, P_TWLN },     // 23 - EMUNAND2 N3DS TWLN
    { 0x800000, TYPE_EMUNAND, P_TWLP },     // 24 - EMUNAND2 N3DS TWLP
    { 0xC00000, TYPE_EMUNAND, P_CTRNAND },  // 25 - EMUNAND3 N3DS CTRNAND
    { 0xC00000, TYPE_EMUNAND, P_TWLN },     // 26 - EMUNAND3 N3DS TWLN
    { 0xC00000, TYPE_EMUNAND, P_TWLP }      // 27 - EMUNAND3 N3DS TWLP
};

    

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
	sdmmc_sdcard_init();
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
    if (DriveInfo[pdrv].type == TYPE_SCARD) {
        if (sdmmc_sdcard_readsectors(sector, count, buff)) {
            return RES_PARERR;
        }
    } else {
        PartitionInfo* partition = GetPartitionInfo(DriveInfo[pdrv].subtype);
        if (partition == NULL) return RES_PARERR;
        u32 offset = (sector * 0x200) + partition->offset;
        SetNand(DriveInfo[pdrv].type == TYPE_EMUNAND, DriveInfo[pdrv].offset);
        if (DecryptNandToMem(buff, offset, count * 0x200, partition) != 0)
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
    if (DriveInfo[pdrv].type == TYPE_SCARD) {
        if (sdmmc_sdcard_writesectors(sector, count, (BYTE *)buff)) {
            return RES_PARERR;
        }
    } else {
        PartitionInfo* partition = GetPartitionInfo(DriveInfo[pdrv].subtype);
        if (partition == NULL) return RES_PARERR;
        u32 offset = (sector * 0x200) + partition->offset;
        SetNand(DriveInfo[pdrv].type == TYPE_EMUNAND, DriveInfo[pdrv].offset);
        // if (EncryptMemToNand(buff, offset, count * 0x200, partition) != 0)
            return RES_PARERR;
        // NO, not yet!
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
            *((DWORD*) buff) = getMMCDevice((DriveInfo[pdrv].type == TYPE_SCARD) ? 1 : 0)->total_size;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *((DWORD*) buff) = 0x2000;
            return (DriveInfo[pdrv].type == TYPE_SCARD) ? RES_OK : RES_PARERR;
        case CTRL_SYNC:
            // nothing to do here - the disk_write function handles that
            return RES_OK;
    }
	return RES_PARERR;
}
#endif
