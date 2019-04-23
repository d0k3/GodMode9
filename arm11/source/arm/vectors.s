/*
 *   This file is part of GodMode9
 *   Copyright (C) 2017-2019 Wolfvak
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arm.h>

.arm
.align 3

.section .vector, "ax"
vectors:
    b XRQ_Reset @ RESET
    b XRQ_Reset @ UNDEFINED
    b XRQ_Reset @ SVC
    b XRQ_Reset @ PREFETCH ABORT
    b XRQ_Reset @ DATA ABORT
    b XRQ_Reset @ RESERVED
    b XRQ_IRQ   @ IRQ
    b XRQ_Reset @ FIQ

XRQ_Reset:
    mov r0, #0x18000000
    add r1, r0, #(6 << 20)
    mov r2, #0xFFFFFFFF
    1:
        cmp r0, r1
        strne r2, [r0], #4
        moveq r0, #0x18000000
        b 1b

XRQ_IRQ:
    sub lr, lr, #4             @ Fix return address
    srsfd sp!, #SR_SVC_MODE    @ Store IRQ mode LR and SPSR on the SVC stack
    cps #SR_SVC_MODE           @ Switch to SVC mode
    push {r0-r4, r12, lr}      @ Preserve registers

    and r4, sp, #7             @ Fix SP to be 8byte aligned
    sub sp, sp, r4

    mov lr, pc
    ldr pc, =GIC_MainHandler

    add sp, sp, r4

    pop {r0-r4, r12, lr}
    rfeia sp!               @ Return from exception
