#include "common.h"
#include "arm.h"
#include "pxi.h"

#define SPIFLASH_CHUNK_SIZE	(0x1000)

static char *spiflash_xfer_buf = NULL;

bool spiflash_get_status(void)
{
	return PXI_DoCMD(PXI_NVRAM_ONLINE, NULL, 0);
}

bool spiflash_read(u32 offset, u32 size, u8 *buf)
{
	u32 args[3];

	if (!spiflash_xfer_buf) {
		u32 xbuf = PXI_DoCMD(PXI_XALLOC, (u32[]){SPIFLASH_CHUNK_SIZE}, 1);
		if (xbuf == 0 || xbuf == 0xFFFFFFFF)
			return false;
		spiflash_xfer_buf = (char*)xbuf;
	}

	args[1] = (u32)spiflash_xfer_buf;

	while(size > 0) {
		u32 blksz = min(size, SPIFLASH_CHUNK_SIZE);

		args[0] = offset;
		args[2] = blksz;

		ARM_DSB();
		PXI_DoCMD(PXI_NVRAM_READ, args, 3);
		ARM_InvDC_Range(spiflash_xfer_buf, blksz); 
		memcpy(buf, spiflash_xfer_buf, blksz);

		buf += blksz;
		size -= blksz;
		offset += blksz;
	}

	return true;
}
