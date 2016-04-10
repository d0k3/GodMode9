#ifdef EXEC_GATEWAY

.section .text.start
.global _start
.align 4
.arm

_vectors:
	ldr pc, =InfiniteLoop
	.pool
	ldr pc, =InfiniteLoop
	.pool
	ldr pc, =InfiniteLoop
	.pool
	ldr pc, =InfiniteLoop
	.pool
	ldr pc, =InfiniteLoop
	.pool
	ldr pc, =InfiniteLoop
	.pool

_start:
	ldr sp,=0x22140000

	@@wait for the arm11 kernel threads to be ready
	ldr r1, =0x10000
	waitLoop9:
		sub r1, #1

		cmp r1, #0
		bgt waitLoop9

	ldr r1, =0x10000
	waitLoop92:
		sub r1, #1

		cmp r1, #0
		bgt waitLoop92

    @ Enable caches
    mrc p15, 0, r4, c1, c0, 0  @ read control register
    orr r4, r4, #(1<<18)       @ - itcm enable
    orr r4, r4, #(1<<12)       @ - instruction cache enable
    orr r4, r4, #(1<<2)        @ - data cache enable
    orr r4, r4, #(1<<0)        @ - mpu enable
    mcr p15, 0, r4, c1, c0, 0  @ write control register
    
	ldr sp,=0x22160000
	ldr	r3, =main
	blx r3
.pool

InfiniteLoop:
	b InfiniteLoop

#endif // EXEC_GATEWAY
