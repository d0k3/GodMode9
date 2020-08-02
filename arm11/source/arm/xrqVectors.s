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

/*
 * This is almost the same as the ARM9 exception handler,
 * but with a few extra register dumps (DFSR, IFSR and FAR)
 */

#include <arm.h>

.arm
.align 3

.macro TRAP_ENTRY xrq
    msr cpsr_f, #(\xrq << 29)
    b xrqMain
.endm

xrqVectorTable:
    ldr pc, =xrqReset
    ldr pc, =xrqUndefined
    ldr pc, =xrqSVC
    ldr pc, =xrqPrefetchAbort
    ldr pc, =xrqDataAbort
    b . @ ignore the reserved exception
    ldr pc, =xrqIRQ
    ldr pc, =xrqFIQ
.pool
xrqVectorTableEnd:

xrqReset:
    TRAP_ENTRY 0

xrqUndefined:
    TRAP_ENTRY 1

xrqSVC:
    TRAP_ENTRY 2

xrqPrefetchAbort:
    TRAP_ENTRY 3

xrqDataAbort:
    TRAP_ENTRY 4

xrqFIQ:
    TRAP_ENTRY 7

xrqMain:
    clrex
    cpsid aif

    ldr sp, =(xrqStackTop - 32*4)
    stmia sp, {r0-r7}

    mrs r1, cpsr
    lsr r0, r1, #29

    mrs r2, spsr
    str lr, [sp, #15*4]
    str r2, [sp, #16*4]

    ands r2, r2, #SR_PMODE_MASK
    orreq r2, r2, #SR_SYS_MODE
    orr r2, r2, #(0x10 | SR_NOINT)

    add r3, sp, #8*4
    msr cpsr_c, r2
    stmia r3!, {r8-r14}
    msr cpsr_c, r1

    mrc p15, 0, r4, c5, c0, 0 @ data fault status register
    mrc p15, 0, r5, c5, c0, 1 @ instruction fault status register
    mrc p15, 0, r6, c6, c0, 0 @ data fault address
    add r3, r3, #2*4 @ skip saved PC and CPSR
    stmia r3!, {r4, r5, r6}

    mov r1, sp
    bl do_exception


xrqIRQ:
    clrex
    sub lr, lr, #4             @ Fix return address
    srsfd sp!, #SR_SVC_MODE    @ Store IRQ mode LR and SPSR on the SVC stack
    cps #SR_SVC_MODE           @ Switch to SVC mode
    push {r0-r4, r12, lr}      @ Preserve registers

    and r4, sp, #7             @ Fix SP to be 8byte aligned
    sub sp, sp, r4

    bl gicTopHandler

    add sp, sp, r4

    pop {r0-r4, r12, lr}
    rfeia sp!               @ Return from exception

@ u32 xrqInstallVectorTable(void)
.global xrqInstallVectorTable
.type xrqInstallVectorTable, %function
xrqInstallVectorTable:
    ldr r0, =xrqPage
    ldr r1, =xrqVectorTable
    mov r2, #(xrqVectorTableEnd - xrqVectorTable)
    b memcpy

.section .bss.xrqPage
.align 12
.global xrqPage
xrqPage:
    .space 8192 @ reserve two 4K aligned pages for vectors and abort stack
.global xrqStackTop
xrqStackTop:
