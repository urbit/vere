


































































	.text
	.align	3
	.globl	___gmpn_lshift 
	
___gmpn_lshift:
	add	x16, x0, x2, lsl #3
	add	x1, x1, x2, lsl #3
	sub	x8, xzr, x3
	lsr	x18, x2, #2
	tbz	x2, #0, Lbx0

Lbx1:	ldr	x4, [x1,#-8]
	tbnz	x2, #1, Lb11

Lb01:	lsr	x0, x4, x8
	lsl	x2, x4, x3
	cbnz	x18, Lgt1
	str	x2, [x16,#-8]
	ret
Lgt1:	ldp	x4, x5, [x1,#-24]
	sub	x1, x1, #8
	add	x16, x16, #16
	b	Llo2

Lb11:	lsr	x0, x4, x8
	lsl	x2, x4, x3
	ldp	x6, x7, [x1,#-24]!
	b	Llo3

Lbx0:	ldp	x4, x5, [x1,#-16]
	tbz	x2, #1, Lb00

Lb10:	lsr	x0, x5, x8
	lsl	x13, x5, x3
	lsr	x10, x4, x8
	lsl	x2, x4, x3
	cbnz	x18, Lgt2
	orr	x10, x10, x13
	stp	x2, x10, [x16,#-16]
	ret
Lgt2:	ldp	x4, x5, [x1,#-32]
	orr	x10, x10, x13
	str	x10, [x16,#-8]
	sub	x1, x1, #16
	add	x16, x16, #8
	b	Llo2

Lb00:	lsr	x0, x5, x8
	lsl	x13, x5, x3
	lsr	x10, x4, x8
	lsl	x2, x4, x3
	ldp	x6, x7, [x1,#-32]!
	orr	x10, x10, x13
	str	x10, [x16,#-8]!
	b	Llo0

	.align	4
Ltop:	ldp	x4, x5, [x1,#-16]
	orr	x10, x10, x13
	orr	x11, x12, x2
	stp	x10, x11, [x16,#-16]
	lsl	x2, x6, x3
Llo2:	lsr	x10, x4, x8
	lsl	x13, x5, x3
	lsr	x12, x5, x8
	ldp	x6, x7, [x1,#-32]!
	orr	x10, x10, x13
	orr	x11, x12, x2
	stp	x10, x11, [x16,#-32]!
	lsl	x2, x4, x3
Llo0:	sub	x18, x18, #1
Llo3:	lsr	x10, x6, x8
	lsl	x13, x7, x3
	lsr	x12, x7, x8
	cbnz	x18, Ltop

Lend:	orr	x10, x10, x13
	orr	x11, x12, x2
	lsl	x2, x6, x3
	stp	x10, x11, [x16,#-16]
	str	x2, [x16,#-24]
	ret
	
