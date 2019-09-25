#include "common.h"
#include "arm.h"

#include "i2c.h"
#include "pxi.h"

// buffer is allocated only once and remains through runtime
static char *i2c_xfer_buf = NULL;

static bool I2C_AllocBuffer(void)
{
	if (!i2c_xfer_buf) {
		u32 xbuf = PXI_DoCMD(PXI_XALLOC, (u32[]){256}, 1);
		if (xbuf == 0 || xbuf == 0xFFFFFFFF)
			return false;
		i2c_xfer_buf = (char*)xbuf;
	}

	return true;
}

bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
	int ret;
	u32 *args;

	if (!I2C_AllocBuffer())
		return false;

	args = (u32[]){devId, regAddr, (u32)i2c_xfer_buf, size};

	ARM_WbDC_Range(i2c_xfer_buf, size);
	ARM_DSB();

	ret = PXI_DoCMD(PXI_I2C_READ, args, 4);

	ARM_InvDC_Range(i2c_xfer_buf, size);
	memcpy(out, i2c_xfer_buf, size);
	return ret;
}

bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size)
{
	int ret;
	u32 *args;

	if (!I2C_AllocBuffer())
		return false;

	args = (u32[]){devId, regAddr, (u32)i2c_xfer_buf, size};

	memcpy(i2c_xfer_buf, in, size);
	ARM_WbDC_Range(i2c_xfer_buf, size);
	ARM_DSB();

	ret = PXI_DoCMD(PXI_I2C_WRITE, args, 4);
	return ret;
}

u8 I2C_readReg(I2cDevice devId, u8 regAddr)
{
	u8 data;
	if (!I2C_readRegBuf(devId, regAddr, &data, 1))
		data = 0xFF;
	return data;
}

bool I2C_writeReg(I2cDevice devId, u8 regAddr, u8 data)
{
	return I2C_writeRegBuf(devId, regAddr, &data, 1);
}
