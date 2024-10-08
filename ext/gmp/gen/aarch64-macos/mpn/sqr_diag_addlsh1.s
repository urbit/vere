





















































	.text
	.align	3
	.globl	___gmpn_sqr_diag_addlsh1 
	
___gmpn_sqr_diag_addlsh1:
	ldr	x15, [x2],#8
	lsr	x18, x3, #1
	tbz	x3, #0, Lbx0

Lbx1:	adds	x7, xzr, xzr
	mul	x12, x15, x15
	ldr	x16, [x2],#8
	ldp	x4, x5, [x1],#16
	umulh	x11, x15, x15
	b	Lmid

Lbx0:	adds	x5, xzr, xzr
	mul	x12, x15, x15
	ldr	x17, [x2],#16
	ldp	x6, x7, [x1],#32
	umulh	x11, x15, x15
	sub	x18, x18, #1
	cbz	x18, Lend

	.align	4
Ltop:	extr	x9, x6, x5, #63
	mul	x10, x17, x17
	ldr	x16, [x2,#-8]
	adcs	x13, x9, x11
	ldp	x4, x5, [x1,#-16]
	umulh	x11, x17, x17
	extr	x8, x7, x6, #63
	stp	x12, x13, [x0],#16
	adcs	x12, x8, x10
Lmid:	extr	x9, x4, x7, #63
	mul	x10, x16, x16
	ldr	x17, [x2],#16
	adcs	x13, x9, x11
	ldp	x6, x7, [x1],#32
	umulh	x11, x16, x16
	extr	x8, x5, x4, #63
	stp	x12, x13, [x0],#16
	adcs	x12, x8, x10
	sub	x18, x18, #1
	cbnz	x18, Ltop

Lend:	extr	x9, x6, x5, #63
	mul	x10, x17, x17
	adcs	x13, x9, x11
	umulh	x11, x17, x17
	extr	x8, x7, x6, #63
	stp	x12, x13, [x0]
	adcs	x12, x8, x10
	extr	x9, xzr, x7, #63
	adcs	x13, x9, x11
	stp	x12, x13, [x0,#16]

	ret
	
