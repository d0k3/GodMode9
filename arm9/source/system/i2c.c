#include "common.h"
#include "arm.h"

#include "i2c.h"
#include "pxi.h"
#include "shmem.h"

bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
	int ret;
	const u32 arg = devId | (regAddr << 8) | (size << 16);

	if (size >= I2C_SHARED_BUFSZ)
		return false;

	ret = PXI_DoCMD(PXI_I2C_READ, &arg, 1);

	ARM_InvDC_Range(ARM_GetSHMEM()->i2cBuffer, size);
	memcpy(out, ARM_GetSHMEM()->i2cBuffer, size);
	return ret;
}

bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size)
{
	int ret;
	const u32 arg = devId | (regAddr << 8) | (size << 16);

	if (size >= I2C_SHARED_BUFSZ)
		return false;

	ARM_InvDC_Range(ARM_GetSHMEM()->i2cBuffer, size);
	memcpy(ARM_GetSHMEM()->i2cBuffer, in, size);
	ARM_WbDC_Range(ARM_GetSHMEM()->i2cBuffer, size);
	ARM_DSB();

	ret = PXI_DoCMD(PXI_I2C_WRITE, &arg, 1);
	return ret;
}

u8 I2C_readReg(I2cDevice devId, u8 regAddr)
{
	u8 data = 0xFF;
	I2C_readRegBuf(devId, regAddr, &data, 1);
	return data;
}

bool I2C_writeReg(I2cDevice devId, u8 regAddr, u8 data)
{
	return I2C_writeRegBuf(devId, regAddr, &data, 1);
}
