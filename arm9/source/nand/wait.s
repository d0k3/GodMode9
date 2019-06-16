.text
.arm
.align 4

.global wait
.type wait, %function
wait:
	subs r0, r0, #4
	bcs wait
	bx lr
