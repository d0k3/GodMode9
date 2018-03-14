.section .text
.arm

#include <arm.h>

#define MPCORE_PMR   (0x17E00000)
#define IRQ_SPURIOUS (1023)
#define IRQ_COUNT    (128)

.global main_irq_handler
.type main_irq_handler, %function
main_irq_handler:
    sub lr, lr, #4             @ Fix return address
    srsfd sp!, #0x13           @ Store IRQ mode LR and SPSR on the SVC stack
    cps #0x13                  @ Switch to SVC mode
    push {r0-r3, r12, lr}      @ Preserve registers

    1:
    ldr lr, =MPCORE_PMR
    ldr r0, [lr, #0x10C]       @ Get pending interrupt

    ldr r1, =IRQ_SPURIOUS
    cmp r0, r1
    beq 3f                     @ Spurious interrupt, no interrupts pending

    cmp r0, #IRQ_COUNT
    bhs 2f                     @ Invalid interrupt ID

    ldr r12, =GIC_Handlers
    ldr r12, [r12, r0, lsl #2]
    cmp r12, #0
    beq 2f

    push {r0, r12}
    blx r12
    pop {r0, r12}

    2:
    ldr lr, =MPCORE_PMR
    str r0, [lr, #0x110]       @ End of interrupt
    b 1b                       @ Check for any other pending interrupts

    3:
    pop {r0-r3, r12, lr}       @ Restore registers
    rfeia sp!                  @ Return From Exception

