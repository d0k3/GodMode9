.section .text.boot
.align 4

#include <arm.h>

#define STACK_SZ    (8192)

.global __boot
__boot:
	cpsid aif, #SR_SVC_MODE

    @ Writeback and invalidate all DCache
    @ Invalidate all caches
    @ Data Synchronization Barrier
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 0
    mcr p15, 0, r0, c7, c7, 0
    mcr p15, 0, r0, c7, c10, 4

    @ Reset control registers
    ldr r0, =0x00054078
    ldr r1, =0x0000000F
    ldr r2, =0x00F00000

    mcr p15, 0, r0, c1, c0, 0
    mcr p15, 0, r1, c1, c0, 1
    mcr p15, 0, r2, c1, c0, 2

    @ Get CPU ID
    mrc p15, 0, r12, c0, c0, 5
    ands r12, r12, #3

    @ Setup stack according to CPU ID
    ldr sp, =(_stack_base + STACK_SZ)
    ldr r0, =STACK_SZ
    mla sp, r0, r12, sp

    beq corezero_start

    cmp r12, #MAX_CPU
    blo coresmp_start

1:
    wfi
    b 1b

corezero_start:
    ldr r0, =__bss_pa
    ldr r1, =__bss_len
    mov r2, #0
    add r1, r0, r1
    .Lclearbss:
        cmp r0, r1
        strlt r2, [r0], #4
        blt .Lclearbss

coresmp_start:
    bl SYS_CoreInit
    bl MPCoreMain

    b __boot

.section .bss.stack
.align 3
_stack_base:
    .space (MAX_CPU * STACK_SZ)
