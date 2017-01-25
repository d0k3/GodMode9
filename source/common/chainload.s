@ Wolfvak - 25/01/2017

@ TODO: ELF launcher
@ void Chainload(u8 *source, size_t size)
@ Wrapper around chainload_itcm

.arm
.global Chainload
.type Chainload, %function
Chainload:
    ldr r2, =0x1FF8100      @ ITCM + 0x100 bytes
    mov r3, r2
    ldr r4, =chainload_itcm
    ldr r5, =chainload_itcm_end

.copy_chainloader:
    cmp r4, r5
    ldrlt r6, [r4], #4
    strlt r6, [r3], #4
    blt .copy_chainloader

    bx r2 @ Branch to the real chainloader in ITCM


@ void chainload_itcm(void)
@ Note: Uses unprotected bootrom functions
.arm
.type chainload_itcm, %function
.align 4
chainload_itcm:
    mov r2, r1
    ldr r1, =0x23F00000

    mov r4, r1 @ memcpy256 and clean_flush_cache mess with the registers

    ldr r3, =0xFFFF03F0 @ memcpy256(u32 *src, u32 *ddest, size_t size)
    blx r3

    ldr r3, =0xFFFF0830 @ void clean_flush_cache(void)
    blx r3

    mov r3, r4

    mov r0, #0 @ Clear argc
    mov r1, #0 @ Same for argv
    mov r2, #0
    mov lr, #0
    bx r3

.pool

chainload_itcm_end:
