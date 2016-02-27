/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "aes.h"
#include "platform.h"
#include "sdmmc.h"

#define TYPE_SDCARD     0
#define TYPE_SYSNAND    1
#define TYPE_EMUNAND    2

#define SUBTYPE_CTRN_O  0
#define SUBTYPE_CTRN_N  1
#define SUBTYPE_TWLN    2
#define SUBTYPE_TWLP    3
#define SUBTYPE_NONE    4

typedef struct {
    DWORD offset;
    BYTE  type;
    BYTE  subtype;
} FATpartition;

typedef struct {
    DWORD offset;
    DWORD mode;
    BYTE  keyslot;
} SubtypeDesc;

FATpartition DriveInfo[31] = {
    { 0x000000, TYPE_SDCARD,  SUBTYPE_NONE },     //   0 - SDCARD
    { 0x000000, TYPE_SYSNAND, SUBTYPE_CTRN_O },   //   1 - SYSNAND CTRNAND
    { 0x000000, TYPE_SYSNAND, SUBTYPE_TWLN },     //   2 - SYSNAND TWLN
    { 0x000000, TYPE_SYSNAND, SUBTYPE_TWLP },     //   3 - SYSNAND TWLP
    { 0x000000, TYPE_EMUNAND, SUBTYPE_CTRN_O },   //   4 - EMUNAND0 O3DS CTRNAND
    { 0x000000, TYPE_EMUNAND, SUBTYPE_TWLN },     //   5 - EMUNAND0 O3DS TWLN
    { 0x000000, TYPE_EMUNAND, SUBTYPE_TWLP },     //   6 - EMUNAND0 O3DS TWLP
    { 0x200000, TYPE_EMUNAND, SUBTYPE_CTRN_O },   //   7 - EMUNAND1 O3DS CTRNAND
    { 0x200000, TYPE_EMUNAND, SUBTYPE_TWLN },     //   8 - EMUNAND1 O3DS TWLN
    { 0x200000, TYPE_EMUNAND, SUBTYPE_TWLP },     //   9 - EMUNAND1 O3DS TWLP
    { 0x400000, TYPE_EMUNAND, SUBTYPE_CTRN_O },   //  10 - EMUNAND2 O3DS CTRNAND
    { 0x400000, TYPE_EMUNAND, SUBTYPE_TWLN },     //  11 - EMUNAND2 O3DS TWLN
    { 0x400000, TYPE_EMUNAND, SUBTYPE_TWLP },     //  12 - EMUNAND2 O3DS TWLP
    { 0x600000, TYPE_EMUNAND, SUBTYPE_CTRN_O },   //  13 - EMUNAND3 O3DS CTRNAND
    { 0x600000, TYPE_EMUNAND, SUBTYPE_TWLN },     //  14 - EMUNAND3 O3DS TWLN
    { 0x600000, TYPE_EMUNAND, SUBTYPE_TWLP },     //  15 - EMUNAND3 O3DS TWLPSDCARD
    { 0x000000, TYPE_SYSNAND, SUBTYPE_CTRN_N },   //  *1 - SYSNAND CTRNAND
    { 0x000000, TYPE_SYSNAND, SUBTYPE_TWLN },     //  *2 - SYSNAND TWLN
    { 0x000000, TYPE_SYSNAND, SUBTYPE_TWLP },     //  *3 - SYSNAND TWLP
    { 0x000000, TYPE_EMUNAND, SUBTYPE_CTRN_N },   //  *4 - EMUNAND0 N3DS CTRNAND
    { 0x000000, TYPE_EMUNAND, SUBTYPE_TWLN },     //  *5 - EMUNAND0 N3DS TWLN 
    { 0x000000, TYPE_EMUNAND, SUBTYPE_TWLP },     //  *6 - EMUNAND0 N3DS TWLP
    { 0x400000, TYPE_EMUNAND, SUBTYPE_CTRN_N },   //  *7 - EMUNAND1 N3DS CTRNAND
    { 0x400000, TYPE_EMUNAND, SUBTYPE_TWLN },     //  *8 - EMUNAND1 N3DS TWLN
    { 0x400000, TYPE_EMUNAND, SUBTYPE_TWLP },     //  *9 - EMUNAND1 N3DS TWLP
    { 0x800000, TYPE_EMUNAND, SUBTYPE_CTRN_N },   // *10 - EMUNAND2 N3DS CTRNAND
    { 0x800000, TYPE_EMUNAND, SUBTYPE_TWLN },     // *11 - EMUNAND2 N3DS TWLN
    { 0x800000, TYPE_EMUNAND, SUBTYPE_TWLP },     // *12 - EMUNAND2 N3DS TWLP
    { 0xC00000, TYPE_EMUNAND, SUBTYPE_CTRN_N },   // *13 - EMUNAND3 N3DS CTRNAND
    { 0xC00000, TYPE_EMUNAND, SUBTYPE_TWLN },     // *14 - EMUNAND3 N3DS TWLN
    { 0xC00000, TYPE_EMUNAND, SUBTYPE_TWLP }      // *15 - EMUNAND3 N3DS TWLP
};

SubtypeDesc SubTypes[4] = {
    { 0x05CAE5, AES_CNT_CTRNAND_MODE, 0x4 }, // O3DS CTRNAND
    { 0x05CAD7, AES_CNT_CTRNAND_MODE, 0x5 }, // N3DS CTRNAND
    { 0x000097, AES_CNT_TWLNAND_MODE, 0x3 }, // TWLN
    { 0x04808D, AES_CNT_TWLNAND_MODE, 0x3 }  // TWLP
};

static bool mode_n3ds = false;
    

/*-----------------------------------------------------------------------*/
/* Get counter for NAND AES decryption                                    */
/*-----------------------------------------------------------------------*/

u32 GetNandCtr(u8* ctr, u32 sector)
{
    // static const char* versions[] = {"4.x", "5.x", "6.x", "7.x", "8.x", "9.x"};
    static const u8* version_ctrs[] = {
        (u8*)0x080D7CAC,
        (u8*)0x080D858C,
        (u8*)0x080D748C,
        (u8*)0x080D740C,
        (u8*)0x080D74CC,
        (u8*)0x080D794C
    };
    static const u32 version_ctrs_len = sizeof(version_ctrs) / sizeof(u32);
    static u8* ctr_start = NULL;
    
    if (ctr_start == NULL) {
        for (u32 i = 0; i < version_ctrs_len; i++) {
            if (*(u32*)version_ctrs[i] == 0x5C980) {
                ctr_start = (u8*) version_ctrs[i] + 0x30;
            }
        }
        
        // If value not in previous list start memory scanning (test range)
        if (ctr_start == NULL) {
            for (u8* c = (u8*) 0x080D8FFF; c > (u8*) 0x08000000; c--) {
                if (*(u32*)c == 0x5C980 && *(u32*)(c + 1) == 0x800005C9) {
                    ctr_start = c + 0x30;
                    break;
                }
            }
        }
        
        if (ctr_start == NULL) {
            return 1;
        }
    }
    
    // the ctr is stored backwards in memory
    if (sector >= (0x0B100000 / 0x200)) { // CTRNAND/AGBSAVE region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + (0xF - i));
    } else { // TWL region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + 0x88 + (0xF - i));
    }
    
    // increment counter
    add_ctr(ctr, sector * (0x200/0x10));

    return 0;
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
    mode_n3ds = (GetUnitPlatform() == PLATFORM_N3DS);
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
    if ((pdrv > 0) && mode_n3ds) // is this really set at this point?
        pdrv += 15;
    
    BYTE type = DriveInfo[pdrv].type;
    
    if (type == TYPE_SDCARD) {
        if (sdmmc_sdcard_readsectors(sector, count, buff)) {
            return RES_PARERR;
        }
    } else {
        BYTE subtype = DriveInfo[pdrv].subtype;
        DWORD isector = SubTypes[subtype].offset + sector;
        DWORD mode = SubTypes[subtype].mode;
        BYTE ctr[16] __attribute__((aligned(32)));
        
        if (type == TYPE_SYSNAND) {
            if (sdmmc_nand_readsectors(isector, count, buff))
                return RES_PARERR;
        } else if (sdmmc_sdcard_readsectors(DriveInfo[pdrv].offset + isector, count, buff)) {
            return RES_PARERR;
        }
        
        GetNandCtr(ctr, isector);
        use_aeskey(SubTypes[subtype].keyslot);
        for (UINT s = 0; s < count; s++) {
            for (UINT b = 0x0; b < 0x200; b += 0x10, buff += 0x10) {
                set_ctr(ctr);
                aes_decrypt((void*) buff, (void*) buff, 1, mode);
                add_ctr(ctr, 0x1);
            }
        }
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
    if ((pdrv > 0) && mode_n3ds)
        pdrv += 15;
    
    if (DriveInfo[pdrv].type == TYPE_SDCARD) {
        if (sdmmc_sdcard_writesectors(sector, count, (BYTE *)buff)) {
            return RES_PARERR;
        }
    } else {
        BYTE subtype = DriveInfo[pdrv].subtype;
        DWORD isector = SubTypes[subtype].offset + sector;
        DWORD mode = SubTypes[subtype].mode;
        BYTE ctr[16] __attribute__((aligned(32)));
        
        GetNandCtr(ctr, isector);
        use_aeskey(SubTypes[subtype].keyslot);
        for (UINT s = 0; s < count; s++) {
            for (UINT b = 0x0; b < 0x200; b += 0x10, buff += 0x10) {
                set_ctr(ctr);
                aes_decrypt((void*) buff, (void*) buff, 1, mode);
                add_ctr(ctr, 0x1);
            }
        }
        
        /*if (type == TYPE_SYSNAND) {
            if (sdmmc_nand_writesectors(isector, count, buff))
                return RES_PARERR;
        } else if (sdmmc_sdcard_writesectors(DriveInfo[pdrv].offset + isector, count, buff)) {
            return RES_PARERR;
        }*/
        // stubbed, better be safe!
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
    if ((pdrv > 0) && mode_n3ds)
        pdrv += 15;
    
    switch (cmd) {
        case GET_SECTOR_SIZE:
            *((DWORD*) buff) = 0x200;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *((DWORD*) buff) = getMMCDevice((DriveInfo[pdrv].type == TYPE_SDCARD) ? 1 : 0)->total_size;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *((DWORD*) buff) = 0x2000;
            // return (DriveInfo[pdrv].type == TYPE_SDCARD) ? RES_OK : RES_PARERR;
            return RES_OK;
        case CTRL_SYNC:
            // nothing to do here - the disk_write function handles that
            return RES_OK;
    }
	return RES_OK;
}
#endif
