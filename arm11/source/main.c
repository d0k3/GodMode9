#include <cpu.h>
#include <pxi.h>
#include <gic.h>
#include <gpulcd.h>
#include <vram.h>
#include <types.h>

#include <i2c.h>

#define CFG11_MPCORE_CLKCNT ((vu16*)(0x10141300))
#define CFG11_SOCINFO ((vu16*)(0x10140FFC))

vu32 *entrypoint = (vu32*)0x1FFFFFFC;

void PXI_RX_Handler(void)
{
    u32 pxi_cmd, ret;
    u32 pxi_args[PXI_FIFO_LEN];

    pxi_cmd = PXI_Recv();
    switch (pxi_cmd) {
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
            PXI_RecvArray(pxi_args, 1);
            LCD_SetBrightness(0, pxi_args[0]);
            LCD_SetBrightness(1, pxi_args[0]);
            ret = pxi_args[0];
            break;
        }

        case PXI_I2C_READ:
        {
            PXI_RecvArray(pxi_args, 4);
            ret = I2C_readRegBuf(pxi_args[0], pxi_args[1],
                (u8*)pxi_args[2], pxi_args[3]);
            break;
        }

        case PXI_I2C_WRITE:
        {
            PXI_RecvArray(pxi_args, 4);
            ret = I2C_writeRegBuf(pxi_args[0], pxi_args[1],
                (const u8*)pxi_args[2], pxi_args[3]);
            break;
        }

        /* New CMD template:
        case CMD_ID:
        {
            <var declarations/assignments>
            <receive args from PXI FIFO>
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
        CPU_EnableIRQ();
        *CFG11_MPCORE_CLKCNT = 0x8001;
        do {
            asm("wfi\n\t");
        } while(!(*CFG11_MPCORE_CLKCNT & 0x8000));
        CPU_DisableIRQ();
    }

    GIC_Reset();

    PXI_Reset();
    I2C_init();
    //MCU_init();

    GIC_SetIRQ(IRQ_PXI_RX, PXI_RX_Handler);

    CPU_EnableIRQ();

    *entrypoint = 0;
    while((entry=*entrypoint) == 0);

    CPU_DisableIRQ();
    GIC_Reset();

    ((void (*)())(entry))();
}
