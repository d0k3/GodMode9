.text
.arm
.align 4

.global wait
.type wait, %function
wait:
	subs r0, r0, #2
	nop
	bgt wait
	bx lr
