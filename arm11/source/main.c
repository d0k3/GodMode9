#include <arm.h>
#include <pxi.h>
#include <gic.h>
#include <gpulcd.h>
#include <vram.h>
#include <types.h>

#include <i2c.h>

#define CFG11_MPCORE_CLKCNT	((vu16*)(0x10141300))
#define CFG11_SOCINFO	((vu16*)(0x10140FFC))

#define LEGACY_BOOT_ENTRY	((vu32*)0x1FFFFFFC)
#define LEGACY_BOOT_MAGIC	(0xDEADDEAD)

void PXI_RX_Handler(void)
{
	u32 ret, msg, cmd, argc, args[PXI_FIFO_LEN];

	msg = PXI_Recv();
	cmd = msg & 0xFFFF;
	argc = msg >> 16;

	if (argc > PXI_FIFO_LEN) {
		PXI_Send(0xFFFFFFFF);
		return;
	}

	PXI_RecvArray(args, argc);

	switch (cmd) {
		case PXI_SCREENINIT:
		{
			GPU_Init();
			GPU_PSCFill(VRAM_START, VRAM_END, 0);
			GPU_SetFramebuffers((u32[]){VRAM_TOP_LA, VRAM_TOP_LB,
										VRAM_TOP_RA, VRAM_TOP_RB,
										VRAM_BOT_A,  VRAM_BOT_B});

			GPU_SetFramebufferMode(0, PDC_RGB24);
			GPU_SetFramebufferMode(1, PDC_RGB24);
			ret = 0;
			break;
		}

		case PXI_BRIGHTNESS:
		{
			LCD_SetBrightness(0, args[0]);
			LCD_SetBrightness(1, args[0]);
			ret = args[0];
			break;
		}

		case PXI_I2C_READ:
		{
			ret = I2C_readRegBuf(args[0], args[1], (u8*)args[2], args[3]);
			break;
		}

		case PXI_I2C_WRITE:
		{
			ret = I2C_writeRegBuf(args[0], args[1], (u8*)args[2], args[3]);
			break;
		}

		case PXI_LEGACY_BOOT:
		{
			*LEGACY_BOOT_ENTRY = LEGACY_BOOT_MAGIC;
			ret = 0;
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
	return;
}

void main(void)
{
	u32 entry;

	if ((*CFG11_SOCINFO & 2) && (!(*CFG11_MPCORE_CLKCNT & 1))) {
		GIC_Reset();
		GIC_SetIRQ(88, NULL);
		arm_enable_ints();
		*CFG11_MPCORE_CLKCNT = 0x8001;
		do {
			asm("wfi\n\t");
		} while(!(*CFG11_MPCORE_CLKCNT & 0x8000));
		arm_disable_ints();
	}

	GIC_Reset();

	PXI_Reset();
	I2C_init();
	//MCU_init();

	GIC_SetIRQ(IRQ_PXI_RX, PXI_RX_Handler);
	*LEGACY_BOOT_ENTRY = 0;

	arm_enable_ints();

	do {
		arm_wfi();
	} while(*LEGACY_BOOT_ENTRY != LEGACY_BOOT_MAGIC);

	arm_disable_ints();
	GIC_Reset();

	do {
		entry = *LEGACY_BOOT_ENTRY;
	} while(entry == LEGACY_BOOT_MAGIC);

	((void (*)())(entry))();
}
