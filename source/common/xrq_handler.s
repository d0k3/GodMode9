/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

.section .text.xrqh
.arm

#include <arm.h>
#include <brf.h>

.macro XRQ_FATAL id=0
    adr sp, XRQ_Registers
    stmia sp!, {r0-r12}
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

XRQ_Registers:
    .space (17*4)

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

XRQ_MainHandler:
    mrs r10, cpsr
    mrs r9, spsr
    mov r8, lr

    @ Disable interrupts
    orr r0, r0, #(SR_IRQ | SR_FIQ)
    msr cpsr, r0

    @ Disable mpu / caches
    ldr r4, =BRF_WB_INV_DCACHE
    ldr r5, =BRF_INVALIDATE_ICACHE
    ldr r6, =BRF_RESETCP15
    blx r4
    blx r5
    blx r6

    @ Retrieve banked registers
    and r0, r9, #(SR_PMODE_MASK)
    cmp r0, #(SR_USR_MODE)
    orreq r0, r9, #(SR_SYS_MODE)
    orr r0, #(SR_IRQ | SR_FIQ)

    msr cpsr_c, r0 @ Switch to previous mode
    mov r0, sp
    mov r1, lr
    msr cpsr, r10  @ Return to abort

    stmia sp!, {r0,r1,r8,r9}

    ldr sp, =__stack_abt
    ldr r2, =XRQ_DumpRegisters
    adr r1, XRQ_Registers
    mov r0, r11
    blx r2

    msr cpsr_c, #(SR_SVC_MODE | SR_IRQ | SR_FIQ)
    mov r0, #0
    .LXRQ_WFI:
        mcr p15, 0, r0, c7, c0, 4
        b .LXRQ_WFI

.pool

.global XRQ_End
XRQ_End:
