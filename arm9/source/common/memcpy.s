@ memcpy_arm946e-s - hand written reimplementation of memcpy to be sequential
@ Written in 2019 by luigoalma <luigoalma at gmail dot com>
@ To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
@ For a copy of CC0 Public Domain Dedication, see <https://creativecommons.org/publicdomain/zero/1.0/>.
	.cpu    arm946e-s
	.arch   armv5te
	.arm
	.section .text.memcpy, "ax", %progbits
	.align  2
	.global memcpy
	.syntax unified
	.type   memcpy, %function
memcpy:
	@ r0 = dest
	@ r1 = src
	@ r2 = length
	@ check if length 0 and return if so
	cmp     r2, #0
	bxeq    lr
	push    {r0,r4-r9,lr}
	@ pre-fetch data
	pld     [r1]
	@ alignment check with word size
	@ if not aligned but both are in the same misalignment, fix it up
	@ otherwise jump to basic loop
	orr     r12, r0, r1
	ands    r12, r12, #3
	beq     .L1
	and     r4, r0, #3
	and     r5, r1, #3
	cmp     r4, r5
	bne     .L6
	rsb     r4, r4, #4
.L0:
	ldrb    r3, [r1], #1
	strb    r3, [r0], #1
	subs    r2, r2, #1
	popeq   {r0,r4-r9,pc}
	subs    r4, r4, #1
	bne     .L0
.L1:
	@ check if length higher than 32
	@ if so, do the 32 byte block copy loop,
	@ until there's nothing left or remainder to copy is less than 32
	movs    r3, r2, LSR#5
	beq     .L3
.L2:
	ldm     r1!, {r4-r9,r12,lr}
	stm     r0!, {r4-r9,r12,lr}
	subs    r3, r3, #1
	bne     .L2
	ands    r2, r2, #0x1F
	popeq   {r0,r4-r9,pc}
.L3:
	@ copy in word size the remaining data,
	@ and finish off with basic loop if can't copy all by word size.
	movs    r3, r2, LSR#2
	beq     .L6
.L4:
	ldr     r12, [r1], #4
	str     r12, [r0], #4
	subs    r3, r3, #1
	bne     .L4
	ands    r2, r2, #0x3
.L5: @ the basic loop
	popeq   {r0,r4-r9,pc}
.L6:
	ldrb    r3, [r1], #1
	strb    r3, [r0], #1
	subs    r2, r2, #1
	b       .L5
	.size   memcpy, .-memcpy
