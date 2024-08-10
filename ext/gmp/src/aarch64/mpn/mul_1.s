

























































	.text
	.align	3
	.globl	___gmpn_mul_1c 
	
___gmpn_mul_1c:
	adds	xzr, xzr, xzr		
	b	Lcom
	

	.text
	.align	3
	.globl	___gmpn_mul_1 
	
___gmpn_mul_1:
	adds	x4, xzr, xzr		
Lcom:	lsr	x18, x2, #2
	tbnz	x2, #0, Lbx1

Lbx0:	mov	x11, x4
	tbz	x2, #1, Lb00

Lb10:	ldp	x4, x5, [x1]
	mul	x8, x4, x3
	umulh	x10, x4, x3
	cbz	x18, L2
	ldp	x6, x7, [x1,#16]!
	mul	x9, x5, x3
	b	Lmid-8

L2:	mul	x9, x5, x3
	b	L2e

Lbx1:	ldr	x7, [x1],#8
	mul	x9, x7, x3
	umulh	x11, x7, x3
	adds	x9, x9, x4
	str	x9, [x0],#8
	tbnz	x2, #1, Lb10

Lb01:	cbz	x18, L1

Lb00:	ldp	x6, x7, [x1]
	mul	x8, x6, x3
	umulh	x10, x6, x3
	ldp	x4, x5, [x1,#16]
	mul	x9, x7, x3
	adcs	x12, x8, x11
	umulh	x11, x7, x3
	add	x0, x0, #16
	sub	x18, x18, #1
	cbz	x18, Lend

	.align	4
Ltop:	mul	x8, x4, x3
	ldp	x6, x7, [x1,#32]!
	adcs	x13, x9, x10
	umulh	x10, x4, x3
	mul	x9, x5, x3
	stp	x12, x13, [x0,#-16]
	adcs	x12, x8, x11
	umulh	x11, x5, x3
Lmid:	mul	x8, x6, x3
	ldp	x4, x5, [x1,#16]
	adcs	x13, x9, x10
	umulh	x10, x6, x3
	mul	x9, x7, x3
	stp	x12, x13, [x0],#32
	adcs	x12, x8, x11
	umulh	x11, x7, x3
	sub	x18, x18, #1
	cbnz	x18, Ltop

Lend:	mul	x8, x4, x3
	adcs	x13, x9, x10
	umulh	x10, x4, x3
	mul	x9, x5, x3
	stp	x12, x13, [x0,#-16]
L2e:	adcs	x12, x8, x11
	umulh	x11, x5, x3
	adcs	x13, x9, x10
	stp	x12, x13, [x0]
L1:	adc	x0, x11, xzr
	ret
	
