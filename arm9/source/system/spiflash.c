#include "common.h"
#include "arm.h"
#include "pxi.h"

#define SPIFLASH_CHUNK_SIZE	(0x1000)

static char *spiflash_xalloc_buf = NULL;

bool spiflash_get_status(void)
{
	return PXI_DoCMD(PXI_NVRAM_ONLINE, NULL, 0);
}

bool spiflash_read(u32 offset, u32 size, u8 *buf)
{
	u32 args[3];

	if (!spiflash_xalloc_buf) {
		u32 xbuf = PXI_DoCMD(PXI_XALLOC, (u32[]){SPIFLASH_CHUNK_SIZE}, 1);
		if (xbuf == 0 || xbuf == 0xFFFFFFFF)
			return false;
		spiflash_xalloc_buf = (char*)xbuf;
	}

	args[1] = (u32)spiflash_xalloc_buf;

	while(size > 0) {
		u32 rem = min(size, SPIFLASH_CHUNK_SIZE);

		args[0] = offset;
		args[2] = rem;

		ARM_DSB();
		PXI_DoCMD(PXI_NVRAM_READ, args, 3);
		ARM_InvDC_Range(spiflash_xalloc_buf, rem); 
		memcpy(buf, spiflash_xalloc_buf, rem);

		buf += rem;
		size -= rem;
		offset += rem;
	}

	return true;
}
