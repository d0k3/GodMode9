.section .text.start
.align 4
.arm

#include <arm.h>
#include <brf.h>

@ Make sure to preserve r0-r2
.global _start
_start:
    @ Switch to supervisor mode and disable interrupts
    msr cpsr_c, #(SR_SYS_MODE | SR_IRQ | SR_FIQ)

    @ Short delay (not always necessary, just in case)
    mov r3, #0x40000
    .Lwaitloop:
        subs r3, #1
        bgt .Lwaitloop

    @ Check the load address
    adr r3, _start
    ldr r4, =__start__
    cmp r3, r4
    beq _start_gm

    @ Relocate the binary to the correct location and branch to it
    ldr r5, =__code_size__
    .Lbincopyloop:
        subs r5, #4
        ldrge r6, [r3, r5]
        strge r6, [r4, r5]
        bge .Lbincopyloop

    mov r5, r0
    mov r6, r1
    mov r7, r2
    ldr r3, =BRF_WB_INV_DCACHE
    blx r3
    mov r0, r5
    mov r1, r6
    mov r2, r7

    mov lr, #0
    mcr p15, 0, lr, c7, c5, 0 @ Invalidate ICache

    bx r4

_start_gm:
    ldr sp, =__stack_top

    mov r9, r0      @ argc
    mov r10, r1     @ argv

    ldr r4, =0xBEEF
    lsl r2, #16
    lsr r2, #16
    cmp r2, r4      @ magic word
    movne r9, #0

    @ Disable caches / mpu
    ldr r1, =(CR_ENABLE_MPU | CR_ENABLE_DCACHE | CR_ENABLE_ICACHE | \
              CR_ENABLE_DTCM)
    ldr r2, =(CR_ENABLE_ITCM | CR_CACHE_RROBIN)
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r1
    orr r0, r2
    mcr p15, 0, r0, c1, c0, 0

    @ Clear bss
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
    .Lbss_clr:
        cmp r0, r1
        strlt r2, [r0], #4
        blt .Lbss_clr

    @ Invalidate caches
    mov r5, #0
    mcr p15, 0, r5, c7, c5, 0  @ invalidate I-cache
    mcr p15, 0, r5, c7, c6, 0  @ invalidate D-cache
    mcr p15, 0, r5, c7, c10, 4 @ drain write buffer

    @ Give read/write access to all the memory regions
    ldr r5, =0x33333333
    mcr p15, 0, r5, c5, c0, 2 @ write data access
    mcr p15, 0, r5, c5, c0, 3 @ write instruction access

    @ Sets MPU regions and cache settings
    adr r0, __mpu_regions
    ldmia r0, {r1-r8}
    mov r0, #0b00101101 @ bootrom/itcm/arm9 mem and fcram are cacheable/bufferable
    mcr p15, 0, r1, c6, c0, 0
    mcr p15, 0, r2, c6, c1, 0
    mcr p15, 0, r3, c6, c2, 0
    mcr p15, 0, r4, c6, c3, 0
    mcr p15, 0, r5, c6, c4, 0
    mcr p15, 0, r6, c6, c5, 0
    mcr p15, 0, r7, c6, c6, 0
    mcr p15, 0, r8, c6, c7, 0
    mcr p15, 0, r0, c3, c0, 0	@ Write bufferable 0, 2, 5
    mcr p15, 0, r0, c2, c0, 0	@ Data cacheable 0, 2, 5
    mcr p15, 0, r0, c2, c0, 1	@ Inst cacheable 0, 2, 5

    @ Enable dctm
    ldr r0, =0x3000800A        @ set dtcm
    mcr p15, 0, r0, c9, c1, 0  @ set the dtcm Region Register

    @ Install exception handlers
    ldr r0, =XRQ_Start
    ldr r1, =XRQ_End
    ldr r2, =0x00000000
    .LXRQ_Install:
        cmp r0, r1
        ldrlt r3, [r0], #4
        strlt r3, [r2], #4
        blt .LXRQ_Install

    @ Enable caches / select low exception vectors
    ldr r1, =(CR_ALT_VECTORS | CR_DISABLE_TBIT)
    ldr r2, =(CR_ENABLE_MPU | CR_ENABLE_DCACHE | CR_ENABLE_ICACHE | \
              CR_ENABLE_DTCM)
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r1
    orr r0, r2
    mcr p15, 0, r0, c1, c0, 0

    @ Fixes mounting of SDMC
    ldr r0, =0x10000000
    mov r1, #0x340
    str r1, [r0, #0x20]

    mov r0, r9
    mov r1, r10

    bl main

__mpu_regions:
    .word 0xFFFF001F @ FFFF0000 64k  | bootrom (unprotected / protected)
    .word 0x3000801B @ 30008000 16k  | dtcm
    .word 0x00000035 @ 00000000 128M | itcm
    .word 0x08000029 @ 08000000 2M   | arm9 mem (O3DS / N3DS)
    .word 0x10000029 @ 10000000 2M   | io mem (ARM9 / first 2MB)
    .word 0x20000037 @ 20000000 256M | fcram (O3DS / N3DS)
    .word 0x1FF00027 @ 1FF00000 1M   | dsp / axi wram
    .word 0x1800002D @ 18000000 8M   | vram (+ 2MB)
