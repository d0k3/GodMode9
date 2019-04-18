#pragma once

#include <types.h>

typedef struct {
	u32 *buf;
	u8 len;
	bool read;
} SPI_XferInfo;

int SPI_DoXfer(u32 dev, SPI_XferInfo *xfer, u32 xfer_cnt);

void SPI_Init(void);
