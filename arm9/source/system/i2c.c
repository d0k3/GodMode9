#include "common.h"
#include "arm.h"

#include "i2c.h"
#include "pxi.h"
#include "shmem.h"

bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
	int ret;
	u8 *const dataBuffer = ARM_GetSHMEM()->dataBuffer.b;
	const u32 arg = devId | (regAddr << 8) | (size << 16);

	if (size >= SHMEM_BUFFER_SIZE)
		return false;

	ret = PXI_DoCMD(PXICMD_I2C_OP, &arg, 1);
	ARM_InvDC_Range(dataBuffer, size);

	memcpy(out, dataBuffer, size);
	return ret;
}

bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size)
{
	int ret;
	u8 *const dataBuffer = ARM_GetSHMEM()->dataBuffer.b;
	const u32 arg = devId | (regAddr << 8) | (size << 16) | BIT(31);

	if (size >= SHMEM_BUFFER_SIZE)
		return false;

	memcpy(dataBuffer, in, size);
	ARM_WbDC_Range(dataBuffer, size);
	ARM_DSB();

	ret = PXI_DoCMD(PXICMD_I2C_OP, &arg, 1);
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
