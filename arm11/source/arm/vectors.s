#include <arm.h>

.arm
.section .text
.align 2

@ temporarily use dumb vectors redirected from the bootrom, rather
@ than MMU-mapped pages

.global irq_vector
.type irq_vector, %function
irq_vector:
    sub lr, lr, #4             @ Fix return address
    srsfd sp!, #SR_SVC_MODE    @ Store IRQ mode LR and SPSR on the SVC stack
    cps #SR_SVC_MODE           @ Switch to SVC mode
    push {r0-r4, r12, lr}      @ Preserve registers

    and r4, sp, #7              @ Fix SP to be 8byte aligned
    sub sp, sp, r4

    bl GIC_MainHandler

    add sp, sp, r4

    pop {r0-r4, r12, lr}
    rfeia sp!               @ Return from exception
