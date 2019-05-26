/*
 Written by Wolfvak, specially sublicensed under the GPLv2
 Read LICENSE for more details
*/

.arm

#include <arm.h>
#include <brf.h>
#include "memmap.h"

.macro TRAP_ENTRY xrq_id
    msr cpsr_f, #(\xrq_id << 29) @ preserve xrq id (idea grabbed from fb3ds)
.endm

.section .vectors, "ax"
.global XRQ_Start
XRQ_Start:
    ldr pc, IRQ_Vector
    IRQ_Vector: .word IRQ_Handler
    ldr pc, FIQ_Vector
    FIQ_Vector: .word FIQ_Handler
    ldr pc, SVC_Vector
    SVC_Vector: .word SVC_Handler
    ldr pc, UND_Vector
    UND_Vector: .word UND_Handler
    ldr pc, PABT_Vector
    PABT_Vector: .word PABT_Handler
    ldr pc, DABT_Vector
    DABT_Vector: .word DABT_Handler
.global XRQ_End
XRQ_End:


.section .text.xrqs
IRQ_Handler:
    TRAP_ENTRY 6
    b XRQ_Fatal

FIQ_Handler:
    TRAP_ENTRY 7
    b XRQ_Fatal

SVC_Handler:
    TRAP_ENTRY 2
    b XRQ_Fatal

UND_Handler:
    TRAP_ENTRY 1
    b XRQ_Fatal

PABT_Handler:
    TRAP_ENTRY 3
    b XRQ_Fatal

DABT_Handler:
    sub lr, lr, #4 @ R14_abt = PC + 8, so it needs a small additional fixup
    TRAP_ENTRY 4
    @b XRQ_Fatal

XRQ_Fatal:
    sub lr, lr, #4 @ PC exception fixup

    ldr sp, =(__STACK_ABT_TOP - 18*4) @ Set up abort stack, 8 byte aligned
    stmia sp, {r0-r7}                 @ Preserve non-banked GPRs

    mrs r1, cpsr
    orr r0, r1, #SR_NOINT
    msr cpsr_c, r0        @ Disable interrupts

    lsr r0, r1, #29       @ Retrieve exception source

    mrs r2, spsr
    str lr, [sp, #15*4]
    str r2, [sp, #16*4]   @ Preserve exception PC and CPSR

    ands r2, r2, #SR_PMODE_MASK
    orreq r2, r2, #SR_SYS_MODE     @ Force a switch to system mode if
                                   @ the exception happened in user mode
    orr r2, r2, #(0x10 | SR_NOINT) @ With interrupts disabled

    add r3, sp, #8*4
    msr cpsr_c, r2
    nop
    nop
    stmia r3, {r8-r14}    @ Preserve banked GPRs (R8-R12, SP_xrq, LR_xrq)
    nop
    nop
    msr cpsr_c, r1

    mov r1, sp
    bl XRQ_DumpRegisters @ XRQ_DumpRegisters(exception_number, saved_regs);

    mov r0, #0
    1:
        mcr p15, 0, r0, c7, c0, 4
        b 1b
