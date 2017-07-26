@ This file was kindly provided by Wolfvak - thank you!

.section .text.boot
.arm

.global __boot
__boot:
    @ Disable interrupts and switch to Supervisor
    cpsid aif, #0x13

    @ Writeback and invalidate caches
    mov r0, #0
    mcr p15, 0, r0, c7, c7, 0
    mcr p15, 0, r0, c7, c14, 0
    mcr p15, 0, r0, c7, c10, 4

    ldr sp, =__stack_top

    @ Reset values
    ldr r0, =0x00054078
    ldr r1, =0x0000000F
    ldr r2, =0x00000000

    @ MMU disabled, Caches disabled, other misc crap going on
    mcr p15, 0, r0, c1, c0, 0
    mcr p15, 0, r1, c1, c0, 1
    mcr p15, 0, r2, c1, c0, 2

    bl main

    b __boot
