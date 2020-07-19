#include "common.h"
#include "arm.h"
#include "pxi.h"
#include "shmem.h"

bool spiflash_get_status(void)
{
	return PXI_DoCMD(PXI_NVRAM_ONLINE, NULL, 0);
}

bool spiflash_read(u32 offset, u32 size, u8 *buf)
{
	u32 args[2];

	while(size > 0) {
		u32 blksz = min(size, SPI_SHARED_BUFSZ);

		args[0] = offset;
		args[1] = blksz;

		ARM_WbDC_Range(ARM_GetSHMEM()->spiBuffer, blksz);
		PXI_DoCMD(PXI_NVRAM_READ, args, 2);
		ARM_InvDC_Range(ARM_GetSHMEM()->spiBuffer, blksz); 
		ARM_DSB();
		memcpy(buf, ARM_GetSHMEM()->spiBuffer, blksz);

		buf += blksz;
		size -= blksz;
		offset += blksz;
	}

	return true;
}
