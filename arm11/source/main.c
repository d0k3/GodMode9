#include <common.h>
#include <types.h>
#include <shmem.h>
#include <arm.h>
#include <pxi.h>

#include "arm/gic.h"

#include "hw/hid.h"
#include "hw/gpulcd.h"
#include "hw/i2c.h"
#include "hw/mcu.h"

#include "system/sys.h"

static bool legacy = false;

static GlobalSharedMemory SharedMemory_State;
static const u8 brightness_lvls[] = {
	0x10, 0x17, 0x1E, 0x25,
	0x2C, 0x34, 0x3C, 0x44,
	0x4D, 0x56, 0x60, 0x6B,
	0x79, 0x8C, 0xA7, 0xD2
};
static int prev_bright_lvl = -1;

void VBlank_Handler(u32 __attribute__((unused)) irqn)
{
	int cur_bright_lvl = (MCU_GetVolumeSlider() >> 2);
	cur_bright_lvl %= countof(brightness_lvls);

	if (cur_bright_lvl != prev_bright_lvl) {
		prev_bright_lvl = cur_bright_lvl;
		LCD_SetBrightness(brightness_lvls[cur_bright_lvl]);
	}

	// the state should probably be stored on its own
	// setion without caching enabled, since it must
	// be readable by the ARM9 at all times anyway
	SharedMemory_State.hid_state = HID_GetState();
	ARM_WbDC_Range(&SharedMemory_State, sizeof(SharedMemory_State));
	ARM_DMB();
}

void PXI_RX_Handler(u32 __attribute__((unused)) irqn)
{
	u32 ret, msg, cmd, argc, args[PXI_MAX_ARGS];

	msg = PXI_Recv();
	cmd = msg & 0xFFFF;
	argc = msg >> 16;

	if (argc > PXI_MAX_ARGS) {
		PXI_Send(0xFFFFFFFF);
		return;
	}

	PXI_RecvArray(args, argc);

	switch (cmd) {
		case PXI_LEGACY_MODE:
		{
			// TODO: If SMP is enabled, an IPI should be sent here (with a DSB)
			legacy = true;
			ret = 0;
			break;
		}

		case PXI_GET_SHMEM:
		{
			ret = (u32)&SharedMemory_State;
			break;
		}

		case PXI_I2C_READ:
		{
			ret = I2C_readRegBuf(args[0], args[1], (u8*)args[2], args[3]);
			ARM_WbDC_Range((void*)args[2], args[3]);
			ARM_DMB();
			break;
		}

		case PXI_I2C_WRITE:
		{
			ARM_InvDC_Range((void*)args[2], args[3]);
			ARM_DMB();
			ret = I2C_writeRegBuf(args[0], args[1], (u8*)args[2], args[3]);
			break;
		}

		/* New CMD template:
		case CMD_ID:
		{
			<var declarations/assignments>
			<execute the command>
			<set the return value>
			break;
		}
		*/

		default:
			ret = 0xFFFFFFFF;
			break;
	}

	PXI_Send(ret);
}

void __attribute__((noreturn)) MainLoop(void)
{
	// enable PXI RX interrupt
	GIC_Enable(PXI_RX_INTERRUPT, BIT(0), GIC_HIGHEST_PRIO, PXI_RX_Handler);

	// enable MCU interrupts
	GIC_Enable(MCU_INTERRUPT, BIT(0), GIC_HIGHEST_PRIO + 1, MCU_HandleInterrupts);

	GIC_Enable(VBLANK_INTERRUPT, BIT(0), GIC_HIGHEST_PRIO + 2, VBlank_Handler);

	// ARM9 won't try anything funny until this point
	PXI_Barrier(ARM11_READY_BARRIER);

	// Process IRQs until the ARM9 tells us it's time to boot something else
	do {
		ARM_WFI();
	} while(!legacy);

	SYS_CoreZeroShutdown();
	SYS_CoreShutdown();
}
