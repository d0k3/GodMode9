.section .text
.arm

#include <arm.h>

.global main_irq_handler
.type main_irq_handler, %function
main_irq_handler:
    sub lr, lr, #4             @ Fix return address
    srsdb sp!, #(SR_SVC_MODE)  @ Store IRQ mode LR and SPSR on the SVC stack
    cpsid i, #(SR_SVC_MODE)    @ Switch to SVC mode
    push {r0-r3,r12}           @ Preserve registers
    and r1, sp, #4
    sub sp, sp, r1             @ Word-align stack
    push {r1,lr}

    bl GIC_AckIRQ              @ Acknowledge interrupt, get handler address
    cmp r0, #0
    beq .Lskip_irq

    blx r0                     @ Branch to interrupt handler

    .Lskip_irq:
    pop {r1,lr}
    add sp, sp, r1             @ Restore stack pointer
    pop {r0-r3,lr}             @ Restore registers
    rfeia sp!                  @ Return From Exception
