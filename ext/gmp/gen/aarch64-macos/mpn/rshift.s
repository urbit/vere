


































































	.text
	.align	3
	.globl	___gmpn_rshift 
	
___gmpn_rshift:
	mov	x16, x0
	sub	x8, xzr, x3
	lsr	x18, x2, #2
	tbz	x2, #0, Lbx0

Lbx1:	ldr	x5, [x1]
	tbnz	x2, #1, Lb11

Lb01:	lsl	x0, x5, x8
	lsr	x2, x5, x3
	cbnz	x18, Lgt1
	str	x2, [x16]
	ret
Lgt1:	ldp	x4, x5, [x1,#8]
	sub	x1, x1, #8
	sub	x16, x16, #32
	b	Llo2

Lb11:	lsl	x0, x5, x8
	lsr	x2, x5, x3
	ldp	x6, x7, [x1,#8]!
	sub	x16, x16, #16
	b	Llo3

Lbx0:	ldp	x4, x5, [x1]
	tbz	x2, #1, Lb00

Lb10:	lsl	x0, x4, x8
	lsr	x13, x4, x3
	lsl	x10, x5, x8
	lsr	x2, x5, x3
	cbnz	x18, Lgt2
	orr	x10, x10, x13
	stp	x10, x2, [x16]
	ret
Lgt2:	ldp	x4, x5, [x1,#16]
	orr	x10, x10, x13
	str	x10, [x16],#-24
	b	Llo2

Lb00:	lsl	x0, x4, x8
	lsr	x13, x4, x3
	lsl	x10, x5, x8
	lsr	x2, x5, x3
	ldp	x6, x7, [x1,#16]!
	orr	x10, x10, x13
	str	x10, [x16],#-8
	b	Llo0

	.align	4
Ltop:	ldp	x4, x5, [x1,#16]
	orr	x10, x10, x13
	orr	x11, x12, x2
	stp	x11, x10, [x16,#16]
	lsr	x2, x7, x3
Llo2:	lsl	x10, x5, x8
	lsl	x12, x4, x8
	lsr	x13, x4, x3
	ldp	x6, x7, [x1,#32]!
	orr	x10, x10, x13
	orr	x11, x12, x2
	stp	x11, x10, [x16,#32]!
	lsr	x2, x5, x3
Llo0:	sub	x18, x18, #1
Llo3:	lsl	x10, x7, x8
	lsl	x12, x6, x8
	lsr	x13, x6, x3
	cbnz	x18, Ltop

Lend:	orr	x10, x10, x13
	orr	x11, x12, x2
	lsr	x2, x7, x3
	stp	x11, x10, [x16,#16]
	str	x2, [x16,#32]
	ret
	
