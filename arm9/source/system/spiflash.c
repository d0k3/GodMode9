#include "common.h"
#include "arm.h"
#include "pxi.h"
#include "shmem.h"

bool spiflash_get_status(void)
{
	return PXI_DoCMD(PXICMD_NVRAM_ONLINE, NULL, 0);
}

bool spiflash_read(u32 offset, u32 size, u8 *buf)
{
	u32 *const dataBuffer = ARM_GetSHMEM()->dataBuffer.w;
	u32 args[2];

	while(size > 0) {
		u32 blksz = min(size, SHMEM_BUFFER_SIZE);

		args[0] = offset;
		args[1] = blksz;

		PXI_DoCMD(PXICMD_NVRAM_READ, args, 2);
		ARM_InvDC_Range(dataBuffer, blksz);
		ARM_DSB();

		memcpy(buf, dataBuffer, blksz);

		buf += blksz;
		size -= blksz;
		offset += blksz;
	}

	return true;
}
