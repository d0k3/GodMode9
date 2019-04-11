#include "common.h"
#include "arm.h"

#include "i2c.h"
#include "pxi.h"

/*
 disgusting hack that deserves to die in hell
 ideally all buffers would be able to be accessed
 by the ARM11, but those in ARM9 RAM are inaccessible
 (.data, .rodata & .bss)

 the current hack assumes all buffers in the heap are
 located in FCRAM, which is accessible to both processors
 but it's horrendous, and hopefully temporary
*/

static char *i2c_fcram_buf = NULL;
bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
	if (!i2c_fcram_buf)
		i2c_fcram_buf = malloc(256);

	int ret;
	u32 args[] = {devId, regAddr, (u32)i2c_fcram_buf, size};

	ARM_WbDC_Range(i2c_fcram_buf, size);
	ARM_DSB();

	ret = PXI_DoCMD(PXI_I2C_READ, args, 4);

	ARM_InvDC_Range(i2c_fcram_buf, size);
	memcpy(out, i2c_fcram_buf, size);
	return ret;
}

bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size)
{
	if (!i2c_fcram_buf)
		i2c_fcram_buf = malloc(256);

	int ret;
	u32 args[] = {devId, regAddr, (u32)i2c_fcram_buf, size};

	memcpy(i2c_fcram_buf, in, size);
	ARM_WbDC_Range(i2c_fcram_buf, size);
	ARM_DSB();

	ret = PXI_DoCMD(PXI_I2C_WRITE, args, 4);
	return ret;
}

/*bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
	int ret;
	u32 args[] = {devId, regAddr, (u32)out, size};
	cpu_writeback_invalidate_dc_range(out, size);
	cpu_membarrier();
	ret = PXI_DoCMD(PXI_I2C_READ, args, 4);
	cpu_invalidate_dc_range(out, size);
	return ret;
}*/

u8 I2C_readReg(I2cDevice devId, u8 regAddr)
{
	u8 data;
	if (!I2C_readRegBuf(devId, regAddr, &data, 1))
		data = 0xFF;
	return data;
}

/*bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size)
{
	int ret;
	u32 args[] = {devId, regAddr, (u32)in, size};
	cpu_writeback_dc_range(in, size);
	cpu_membarrier();
	ret = PXI_DoCMD(PXI_I2C_WRITE, args, 4);
	return ret;
}*/

bool I2C_writeReg(I2cDevice devId, u8 regAddr, u8 data)
{
	return I2C_writeRegBuf(devId, regAddr, &data, 1);
}
