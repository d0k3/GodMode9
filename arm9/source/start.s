.section .text.start
.align 4
.arm

#include <arm.h>
#include <brf.h>
#include <entrypoints.h>
#include "memmap.h"

.global _start
_start:
    @ Disable interrupts
    msr cpsr_c, #(SR_SVC_MODE | SR_NOINT)

    @ Preserve boot registers
    mov r8, r0
    mov r9, r1
    mov r10, r2
    @mov r11, r3 @ unnecessary for now

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

    @ Disable caches / TCMs / MPU
    ldr r1, =(CR_MPU | CR_CACHES | CR_DTCM | CR_ITCM | CR_TCM_LOAD)
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r1
    mcr p15, 0, r0, c1, c0, 0

    @ Set access permissions
    ldr r0, =0x11111115 @ RO data access for BootROM, RW otherwise
    ldr r1, =0x00505005 @ Can only execute code from ARM9 RAM, FCRAM and BootROM

    mcr p15, 0, r0, c5, c0, 2
    mcr p15, 0, r1, c5, c0, 3

    @ Set MPU regions and cache settings
    ldr r0, =__mpu_regions
    ldmia r0, {r0-r7}
    mcr p15, 0, r0, c6, c0, 0
    mcr p15, 0, r1, c6, c1, 0
    mcr p15, 0, r2, c6, c2, 0
    mcr p15, 0, r3, c6, c3, 0
    mcr p15, 0, r4, c6, c4, 0
    mcr p15, 0, r5, c6, c5, 0
    mcr p15, 0, r6, c6, c6, 0
    mcr p15, 0, r7, c6, c7, 0

    mov r0, #0b10101000 @ enable write buffer for VRAM
    mcr p15, 0, r0, c3, c0, 0	@ Write bufferable

    mov r0, #0b00101000
    mcr p15, 0, r0, c2, c0, 0	@ Data cacheable
    mcr p15, 0, r0, c2, c0, 1	@ Inst cacheable

    @ Configure TCMs
    ldr r0, =0x3000800A
    ldr r1, =0x00000024
    mcr p15, 0, r0, c9, c1, 0 @ DTCM
    mcr p15, 0, r1, c9, c1, 1 @ ITCM


    @ Setup heap
    ldr r0, =fake_heap_start
    ldr r1, =__HEAP_ADDR
    str r1, [r0]

    ldr r0, =fake_heap_end
    ldr r1, =__HEAP_END
    str r1, [r0]

    @ Install exception handlers
    ldr r0, =__vectors_lma
    ldr r1, =__vectors_len
    ldr r2, =XRQ_Start
    add r1, r0, r1
    .LXRQ_Install:
        cmp r0, r1
        ldrlo r3, [r0], #4
        strlo r3, [r2], #4
        blo .LXRQ_Install

    @ Enable caches / TCMs / select high exception vectors
    ldr r1, =(CR_MPU | CR_CACHES | CR_ITCM | CR_DTCM | CR_ALT_VECTORS)
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r1
    mcr p15, 0, r0, c1, c0, 0

    @ Switch to system mode, disable interrupts, setup application stack
    msr cpsr_c, #(SR_SYS_MODE | SR_NOINT)
    ldr sp, =__STACK_TOP

    @ Check entrypoints
    @ assume by default that the entrypoint
    @ cant be detected and fix up if necessary
    mov r0, #0
    mov r1, #0
    ldr r2, =ENTRY_UNKNOWN

    @ returning from main will trigger a prefetch abort
    mov lr, #0

    @ B9S
    @ if (R2 & 0xFFFF) == 0xBEEF
    ldr r3, =0xBEEF
    lsl r2, r10, #16
    cmp r3, r2, lsr #16

    moveq r0, r8
    moveq r1, r9
    ldreq r2, =ENTRY_B9S
    ldreq pc, =main

    @ ntrboot
    @ if ([0x1FFFE010] | [0x1FFFE014]) == 0
    @ && ([0x1FFFE00C] & 0xFF00FF00) == 0x02000000
    ldr r3, =0x1FFFE010

    ldrd r4, r5, [r3]
    orrs r4, r4, r5
    ldreq r4, [r3, #-4]
    ldreq r5, =0xFF00FF00
    andeq r4, r4, r5
    cmpeq r4, #0x02000000
    ldreq r2, =ENTRY_NTRBOOT
    ldreq pc, =main

    @ nandboot
    @ if ([0x1FFFE010] | [0x1FFFE014]) != 0
    @ && ([0x1FFFE00C] & 0xFF) == 0
    ldrd r4, r5, [r3]
    orrs r4, r4, r5
    ldreq pc, =main

    ldrb r4, [r3, #-4]
    cmp r4, #0
    ldreq r2, =ENTRY_NANDBOOT

    @ unconditionally branch into the main C function
    @ if no entrypoint was detected
    @ R2 will be ENTRY_UNKNOWN
    ldr pc, =main


__mpu_regions:
    .word 0xFFFF001F @ FFFF0000 64k  | bootrom (unprotected / protected)
    .word 0x3000801B @ 30008000 16k  | dtcm
    .word 0x01FF801D @ 01FF8000 32k  | itcm (+ mirrors)
    .word 0x08000029 @ 08000000 2M   | arm9 mem (O3DS / N3DS)
    .word 0x10000029 @ 10000000 2M   | io mem (ARM9 / first 2MB)
    .word 0x20000037 @ 20000000 256M | fcram (O3DS / N3DS)
    .word 0x1FF00027 @ 1FF00000 1M   | dsp / axi wram
    .word 0x1800002D @ 18000000 8M   | vram (+ 2MB)
