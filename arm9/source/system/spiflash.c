#include "common.h"
#include "arm.h"
#include "pxi.h"
#include "shmem.h"

#include "ui.h"

typedef struct {
	u32 id;
	u32 size;
	const char *name;
} spiflash_chip_t;

/**
 * List of known SPI flash chips available in the NDS/3DS family of systems.
 * If it doesn't match any of these, it should be safe to assume we have a 128k flash chip.
 * Obtained from gbatek (https://problemkaputt.de/gbatek-ds-firmware-serial-flash-memory.htm) as of 1/21/26.
 */
static const spiflash_chip_t spiflash_known_chips[] = {
    { 0x124020, 256u << 10, "ST M45PE20" },
    { 0x125020, 256u << 10, "ST M35PE20" },
    { 0x138020, 512u << 10, "ST M25PE40" },
    { 0x114020, 128u << 10, "ST 45PE10V6" },
    { 0x0C5820, 128u << 10, "5A32 (4k)" },
    { 0x0C6262, 128u << 10, "32B/3XH (4k)" },
};

static const spiflash_chip_t *spiflash_initialize(void) {
	static spiflash_chip_t spiflash_chip = {
		.id = 0xFFFFFFFF, /* filled at runtime */
		.size = 128u << 10,
		.name = "Unknown"
	};
	if (spiflash_chip.id != 0xFFFFFFFF)
		return &spiflash_chip;

	u32 id = PXI_DoCMD(PXICMD_NVRAM_ID, NULL, 0);

	spiflash_chip.id = id;

	for (unsigned i = 0; i < countof(spiflash_known_chips); i++) {
		if (id == spiflash_known_chips[i].id) {
			spiflash_chip = spiflash_known_chips[i];
			break;
		}
	}

	return &spiflash_chip;
}

u32 spiflash_size(void)
{
	return spiflash_initialize()->size;
}

bool spiflash_read(u32 offset, u32 size, u8 *buf)
{
	u32 total_size = spiflash_size();
	if ((offset >= total_size) ||
		(size > total_size) ||
		(offset + size > total_size))
		return false;

	u32 *const dataBuffer = ARM_GetSHMEM()->dataBuffer.w;
	u32 args[2];

	while(size > 0) {
		u32 blksz = min(size, SHMEM_BUFFER_SIZE);

		args[0] = offset;
		args[1] = blksz;

		int result = PXI_DoCMD(PXICMD_NVRAM_READ, args, 2);
		if (result != 0) {
			ShowPrompt(false, "SPI flash read failed at offset\n0x%08lx size 0x%08lx (%d)", offset, blksz, result);
			return false;
		}

		ARM_InvDC_Range(dataBuffer, blksz);
		ARM_DSB();

		memcpy(buf, dataBuffer, blksz);

		buf += blksz;
		size -= blksz;
		offset += blksz;
	}

	return true;
}

bool spiflash_write(u32 offset, u32 size, const u8 *buf)
{
	u32 total_size = spiflash_size();
	if ((offset >= total_size) ||
		(size > total_size) ||
		(offset + size > total_size))
		return false;

	u32 *const dataBuffer = ARM_GetSHMEM()->dataBuffer.w;
	u32 args[2];

	while(size > 0) {
		u32 blksz = min(size, SHMEM_BUFFER_SIZE);

		args[0] = offset;
		args[1] = blksz;

		memcpy(dataBuffer, buf, blksz);
		ARM_WbInvDC_Range(dataBuffer, blksz);
		ARM_DSB();

		int result = PXI_DoCMD(PXICMD_NVRAM_WRITE, args, 2);
		if (result != 0) {
			ShowPrompt(false, "SPI flash write failed at offset\n0x%08lx size 0x%08lx (%d)", offset, blksz, result);
			return false;
		}

		buf += blksz;
		size -= blksz;
		offset += blksz;
	}

	return true;
}
