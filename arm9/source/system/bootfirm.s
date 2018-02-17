.section .text
.arm
.align 4

#include <arm.h>
#include <vram.h>

.equ ARG_MAGIC, 0x0003BEEF
.equ MPCORE_LD, 0x27FFFB00
.equ STUB_LOC,  0x27FFFC00
.equ ARGV_LOC,  0x27FFFE00
.equ FBPTR_LOC, 0x27FFFE08
.equ PATH_LOC,  0x27FFFF00

.cpu mpcore
MPCore_stub:
    cpsid aif, #(SR_SVC_MODE)
    mov r0, #0x20000000
    mov r1, #0
    str r1, [r0, #-4]
    .Lcheckmpcentry:
        ldr r1, [r0, #-4]
        cmp r1, #0
        beq .Lcheckmpcentry
    bx r1
.pool
MPCore_stub_end:

@ Assume these functions ALWAYS clobber R0-R3 and R12 and don't use the stack
.equ MEMCPY,   0xFFFF0374 @ void memcpy32(void *src, void *dest, u32 size)
.equ INV_DC,   0xFFFF07F0 @ void invalidate_dcache(void)
.equ WB_DC,    0xFFFF07FC @ void writeback_dcache(void)
.equ WBINV_DC, 0xFFFF0830 @ void writeback_invalidate_dcache(void)
.equ INV_IC,   0xFFFF0AB4 @ void invalidate_icache(void)
.equ INITCP15, 0xFFFF0C58 @ void reset_cp15(void)

.cpu arm946e-s
@ void BootFirm_stub(void *firm, char *path)
@ r0-r8: scratch registers
@ r9: FIRM path
@ r10: FIRM header
@ r11: current section header
BootFirm_stub:
    mov r10, r0
    add r11, r0, #0x40
    mov r9, r1

    mov r4, #4
    .LBootFirm_stub_copysect:
        @ Fetch source, destination and length
        ldmia r11, {r0-r2}

        cmp r2, #0    @ If section is unused/zerolength, don't even bother
        addne r0, r10 @ Fix source address

        ldrne r3, =MEMCPY
        blxne r3

        subs r4, #1
        addne r11, #0x30 @ Advance to the next section
        bne .LBootFirm_stub_copysect

    @ Boot state
        @ CPSR:
        @ ARM, Supervisor, IRQ/FIQs disabled
        @ Flags are undefined
        msr cpsr_c, #(SR_SVC_MODE | SR_IRQ | SR_FIQ)

        @ CP15:
        @ MPU and Caches are off
        @ TCMs are on (location/configuration is undefined)
        @ Alternative exception vectors are enabled (0xFFFF0000)
        ldr r3, =WBINV_DC
        ldr r4, =INV_IC
        ldr r5, =INITCP15
        blx r3
        blx r4
        blx r5

        @ Registers:
        @ R0 = 0x1 or 0x2
        @ R1 = 0x23FFFE10
        @ R2 = 0x0003BEEF
        @ R3-R14 are undefined

        @ Check screen-init flag
        ldrb r3, [r10, #0x10]
        tst r3, #1
        movne r0, #2
        moveq r0, #1
        ldr r1, =ARGV_LOC
        ldr r2, =ARG_MAGIC


    @ Setup argv
    ldrne r3, =FBPTR_LOC
    str r9,   [r1, #0x00] @ FIRM path / argv[0]
    strne r3, [r1, #0x04] @ Framebuffers / argv[1]

    @ Fetch FIRM entrypoints
    ldr r3, [r10, #0x08] @ ARM11 entrypoint
    ldr r4, [r10, #0x0C] @ ARM9 entrypoint

    @ Set the ARM11 entrypoint
    mov r5, #0x20000000
    str r3, [r5, #-4]

    @ Branch to the ARM9 entrypoint
    bx r4

.pool
BootFirm_stub_end:

@ void BootFirm(void *firm, char *path)
@ BootFirm_stub wrapper
@ No checks are performed on the data
.global BootFirm
.type BootFirm, %function
BootFirm:
    mov r10, r0
    mov r11, r1

    @ Setup the framebuffer struct
    ldr r0, =FBPTR_LOC
    ldr r1, =VRAM_TOP_LA
    ldr r2, =VRAM_TOP_RA
    ldr r3, =VRAM_BOT_A
    stmia r0!, {r1,r2,r3}

    ldr r1, =VRAM_TOP_LB
    ldr r2, =VRAM_TOP_RB
    ldr r3, =VRAM_BOT_B
    stmia r0!, {r1,r2,r3}

    @ Copy the FIRM path somewhere safe
    ldr r0, =PATH_LOC
    mov r1, r11
    mov r11, r0
    blx strcpy

    @ Relocate the MPCore stub binary
    ldr r4, =MPCORE_LD
    adr r1, MPCore_stub
    adr r2, MPCore_stub_end
    sub r2, r1
    mov r0, r4
    blx memcpy

    @ Make the ARM11 run the stub, wait until its done
    mov r1, #0x20000000
    mov r0, r4
    str r0, [r1, #-4]
    .Lwaitforsi:
        ldr r0, [r1, #-4]
        cmp r0, #0
        bne .Lwaitforsi

    @ Relocate BootFirm
    ldr r4, =STUB_LOC
    adr r5, BootFirm_stub
    adr r6, BootFirm_stub_end
    sub r7, r6, r5

    mov r0, r4
    mov r1, r5
    mov r2, r7
    blx memcpy

    mov r0, r10
    mov r1, r11

    bx r4
    b .
