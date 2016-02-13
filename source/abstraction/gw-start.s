#ifdef EXEC_GATEWAY

.section ".init"

.global _start
.extern main

.align	4
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

	ldr sp,=0x22160000
	ldr	r3, =main
	blx r3
.pool

InfiniteLoop:
	b InfiniteLoop

#endif // EXEC_GATEWAY
