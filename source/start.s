.section .text.start
.align 4
.arm

#include <arm.h>
#include <brf.h>

.global _start
_start:
    @ Disable interrupts
    mrs r4, cpsr
    orr r4, r4, #(SR_IRQ | SR_FIQ)
    msr cpsr_c, r4

    @ Preserve boot registers
    mov r9, r0
    mov r10, r1
    mov r11, r2

    @ Clear bss
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
    .LBSS_Clear:
        cmp r0, r1
        strlo r2, [r0], #4
        blo .LBSS_Clear

    ldr r0, =BRF_WB_INV_DCACHE
    blx r0 @ Writeback & Invalidate Data Cache
    ldr r0, =BRF_INVALIDATE_ICACHE
    blx r0 @ Invalidate Instruction Cache

    @ Disable caches / DTCM / MPU
    ldr r1, =(CR_ENABLE_MPU | CR_ENABLE_DCACHE | CR_ENABLE_ICACHE | \
              CR_ENABLE_DTCM)
    ldr r2, =(CR_ENABLE_ITCM)
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r1
    orr r0, r2
    mcr p15, 0, r0, c1, c0, 0

    @ Give full access to defined memory regions
    ldr r0, =0x33333333
    mcr p15, 0, r0, c5, c0, 2 @ write data access
    mcr p15, 0, r0, c5, c0, 3 @ write instruction access

    @ Set MPU regions and cache settings
    adr r0, __mpu_regions
    ldmia r0, {r1-r8}
    mov r0, #0b00101000
    mcr p15, 0, r1, c6, c0, 0
    mcr p15, 0, r2, c6, c1, 0
    mcr p15, 0, r3, c6, c2, 0
    mcr p15, 0, r4, c6, c3, 0
    mcr p15, 0, r5, c6, c4, 0
    mcr p15, 0, r6, c6, c5, 0
    mcr p15, 0, r7, c6, c6, 0
    mcr p15, 0, r8, c6, c7, 0
    mcr p15, 0, r0, c3, c0, 0	@ Write bufferable
    mcr p15, 0, r0, c2, c0, 0	@ Data cacheable
    mcr p15, 0, r0, c2, c0, 1	@ Inst cacheable

    @ Enable dctm
    ldr r0, =0x3000800A
    mcr p15, 0, r0, c9, c1, 0  @ set the DTCM Region Register

    @ Enable caches / select low exception vectors
    ldr r1, =(CR_ALT_VECTORS | CR_DISABLE_TBIT)
    ldr r2, =(CR_ENABLE_MPU  | CR_ENABLE_DCACHE | CR_ENABLE_ICACHE | \
              CR_ENABLE_DTCM | CR_CACHE_RROBIN)
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r1
    orr r0, r2
    mcr p15, 0, r0, c1, c0, 0

    @ Install exception handlers
    ldr r0, =XRQ_Start
    ldr r1, =XRQ_End
    ldr r2, =0x00000000
    .LXRQ_Install:
        cmp r0, r1
        ldrlo r3, [r0], #4
        strlo r3, [r2], #4
        blo .LXRQ_Install

    @ Fix SDMC mounting
    mov r0, #0x10000000
    mov r1, #0x340
    str r1, [r0, #0x20]

    @ Check arguments
    lsl r2, r11, #16
    lsr r2, r2, #16

    ldr r3, =0xBEEF
    cmp r2, r3

    moveq r0, r9
    moveq r1, r10
    movne r0, #0

    @ Switch to system mode, disable interrupts, setup application stack
    msr cpsr_c, #(SR_SYS_MODE | SR_IRQ | SR_FIQ)
    ldr sp, =__stack_top
    b main

__mpu_regions:
    .word 0xFFFF001F @ FFFF0000 64k  | bootrom (unprotected / protected)
    .word 0x3000801B @ 30008000 16k  | dtcm
    .word 0x00000035 @ 00000000 128M | itcm
    .word 0x08000029 @ 08000000 2M   | arm9 mem (O3DS / N3DS)
    .word 0x10000029 @ 10000000 2M   | io mem (ARM9 / first 2MB)
    .word 0x20000037 @ 20000000 256M | fcram (O3DS / N3DS)
    .word 0x1FF00027 @ 1FF00000 1M   | dsp / axi wram
    .word 0x1800002D @ 18000000 8M   | vram (+ 2MB)
