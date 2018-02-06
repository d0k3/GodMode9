/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

.section .text.xrqh
.arm

#include <arm.h>
#include <brf.h>
#include "memmap.h"

.macro XRQ_FATAL id=0
    ldr sp, =__STACK_ABT_TOP
    sub sp, sp, #(18*4)  @ Reserve space for registers
    stmia sp, {r0-r12}
    mov r11, #\id
    b XRQ_MainHandler
.endm

.global XRQ_Start
XRQ_Start:
XRQ_Vectors:
    b XRQ_Reset
    b XRQ_Undefined
    b XRQ_SWI
    b XRQ_PAbort
    b XRQ_DAbort
    b .             @ Reserved exception vector
    subs pc, lr, #4 @ IRQs are unhandled
    b .             @ FIQs are unused (except for debug?)

XRQ_Reset:
    msr cpsr_c, #(SR_ABT_MODE | SR_IRQ | SR_FIQ)
    XRQ_FATAL 0

XRQ_Undefined:
    XRQ_FATAL 1

XRQ_SWI:
    XRQ_FATAL 2

XRQ_PAbort:
    XRQ_FATAL 3

XRQ_DAbort:
    XRQ_FATAL 4

@ r11 = exception number
XRQ_MainHandler:
    mrs r10, cpsr
    mrs r9, spsr
    mov r8, lr

    @ Disable mpu / caches
    ldr r4, =BRF_WB_INV_DCACHE
    ldr r5, =BRF_INVALIDATE_ICACHE
    ldr r6, =BRF_RESETCP15
    blx r4
    blx r5
    blx r6

    @ Retrieve banked registers
    ands r0, r9, #(SR_PMODE_MASK & (0x0F))
    orreq r0, #(SR_SYS_MODE)
    orr r0, #(0x10 | SR_IRQ | SR_FIQ)

    msr cpsr_c, r0   @ Switch to previous mode
    mov r0, sp
    mov r1, lr
    msr cpsr_c, r10  @ Return to abort

    add r2, sp, #(13*4)
    stmia r2, {r0,r1,r8,r9}

    @ Give read/write access to all the memory regions
    ldr r0, =0x33333333
    mcr p15, 0, r0, c5, c0, 2 @ write data access
    mcr p15, 0, r0, c5, c0, 3 @ write instruction access

    @ Sets MPU regions and cache settings
    adr r0, __abt_mpu_regions
    ldmia r0, {r1-r8}
    mov r0, #0b00110010 @ bootrom, arm9 mem and fcram are cacheable/bufferable
    mcr p15, 0, r1, c6, c0, 0
    mcr p15, 0, r2, c6, c1, 0
    mcr p15, 0, r3, c6, c2, 0
    mcr p15, 0, r4, c6, c3, 0
    mcr p15, 0, r5, c6, c4, 0
    mcr p15, 0, r6, c6, c5, 0
    mcr p15, 0, r7, c6, c6, 0
    mcr p15, 0, r8, c6, c7, 0
    mcr p15, 0, r0, c3, c0, 0   @ Write bufferable 0, 2, 5
    mcr p15, 0, r0, c2, c0, 0   @ Data cacheable 0, 2, 5
    mcr p15, 0, r0, c2, c0, 1   @ Inst cacheable 0, 2, 5

    @ Enable mpu/caches
    ldr r1, =(CR_ENABLE_MPU | CR_ENABLE_DCACHE | CR_ENABLE_ICACHE | CR_ENABLE_DTCM)
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, r1
    mcr p15, 0, r0, c1, c0, 0

    ldr r2, =XRQ_DumpRegisters @ void XRQ_DumpRegisters(u32 xrq_id, u32 *regs)
    mov r1, sp
    mov r0, r11
    blx r2

    msr cpsr, #(SR_SVC_MODE | SR_IRQ | SR_FIQ)
    mov r0, #0
    .LXRQ_WFI:
        mcr p15, 0, r0, c7, c0, 4
        b .LXRQ_WFI

.pool

__abt_mpu_regions:
    .word 0x0000003F @ 00000000 4G   | background region (includes IO regs)
    .word 0xFFFF001F @ FFFF0000 64k  | bootrom (unprotected / protected)
    .word 0x3000801B @ 30008000 16k  | dtcm
    .word 0x00000035 @ 00000000 128M | itcm
    .word 0x08000029 @ 08000000 2M   | arm9 mem (O3DS / N3DS)
    .word 0x20000037 @ 20000000 256M | fcram (O3DS / N3DS)
    .word 0x1FF00027 @ 1FF00000 1M   | dsp / axi wram
    .word 0x1800002D @ 18000000 8M   | vram (+ 2MB)

.global XRQ_End
XRQ_End:
