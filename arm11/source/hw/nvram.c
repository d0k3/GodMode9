#include <types.h>

#include "hw/spi.h"
#include "hw/nvram.h"

#define NVRAM_SPI_DEV	1

// returns manuf id, memory type and size
// size = (1 << id[2]) ?
// apparently unreliable on some Sanyo chips?
#define CMD_RDID	0x9F

#define CMD_READ	0x03
#define CMD_WREN	0x06
#define CMD_WRDI	0x04

#define CMD_RDSR	0x05

#define CMD_DPD	0xB9 // deep power down
#define CMD_RDP	0xAB // release from deep power down

static u32 NVRAM_SendStatusCommand(u32 cmd, u32 width)
{
	u32 ret;
	SPI_XferInfo xfer[2];

	xfer[0].buf = &cmd;
	xfer[0].len = 1;
	xfer[0].read = false;

	xfer[1].buf = &ret;
	xfer[1].len = width;
	xfer[1].read = true;

	ret = 0;
	SPI_DoXfer(NVRAM_SPI_DEV, xfer, 2);
	return ret;
}

u32 NVRAM_Status(void)
{
	return NVRAM_SendStatusCommand(CMD_RDSR, 1);
}

u32 NVRAM_ReadID(void)
{
	return NVRAM_SendStatusCommand(CMD_RDID, 3);
}

void NVRAM_DeepStandby(void)
{
	NVRAM_SendStatusCommand(CMD_DPD, 0);
}

void NVRAM_Wakeup(void)
{
	NVRAM_SendStatusCommand(CMD_RDP, 0);
}

void NVRAM_Read(u32 address, u32 *buffer, u32 len)
{
	SPI_XferInfo xfer[2];
	u32 cmd;

	address &= BIT(24) - 1;
	cmd = __builtin_bswap32(address) | CMD_READ;

	xfer[0].buf = &cmd;
	xfer[0].len = 4;
	xfer[0].read = false;

	xfer[1].buf = buffer;
	xfer[1].len = len;
	xfer[1].read = true;

	SPI_DoXfer(NVRAM_SPI_DEV, xfer, 2);
}
