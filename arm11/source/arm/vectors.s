#include <arm.h>

.arm
.align 2

.section .vector, "ax"
vectors:
    ldr pc, =XRQ_Reset
    ldr pc, =XRQ_Reset
    ldr pc, =XRQ_Reset
    ldr pc, =XRQ_Reset
    ldr pc, =XRQ_Reset
    b . @ RESERVED
    ldr pc, =XRQ_IRQ
    ldr pc, =XRQ_Reset
.pool

.section .text.XRQS, "ax"

XRQ_Reset:
    mov r0, #0x18000000
    add r1, r0, #(6 << 20)
    mov r2, #0xFFFFFFFF
    1:
        cmp r0, r1
        strne r2, [r0], #4
        bne 1b
    2:
        wfi
        b 2b

.global XRQ_IRQ
XRQ_IRQ:
    sub lr, lr, #4             @ Fix return address
    srsfd sp!, #SR_SVC_MODE    @ Store IRQ mode LR and SPSR on the SVC stack
    cps #SR_SVC_MODE           @ Switch to SVC mode
    push {r0-r4, r12, lr}      @ Preserve registers

    and r4, sp, #7             @ Fix SP to be 8byte aligned
    sub sp, sp, r4

    bl GIC_MainHandler

    add sp, sp, r4

    pop {r0-r4, r12, lr}
    rfeia sp!               @ Return from exception
