





























































  
  
  
  



	.text
	.align	3
	.globl	___gmpn_submul_1 
	
___gmpn_submul_1:
	adds	x15, xzr, xzr

	tbz	x2, #0, L1

	ldr	x4, [x1],#8
	mul	x8, x4, x3
	umulh	x12, x4, x3
	ldr	x4, [x0]
	subs	x8, x4, x8
	csinc	x15, x12, x12, cs
	str	x8, [x0],#8

L1:	tbz	x2, #1, L2

	ldp	x4, x5, [x1],#16
	mul	x8, x4, x3
	umulh	x12, x4, x3
	mul	x9, x5, x3
	umulh	x13, x5, x3
	adds	x8, x8, x15
	adcs	x9, x9, x12
	ldp	x4, x5, [x0]
	adc	x15, x13, xzr
	subs	x8, x4, x8
	sbcs	x9, x5, x9
	csinc	x15, x15, x15, cs
	stp	x8, x9, [x0],#16

L2:	lsr	x2, x2, #2
	cbz	x2, Lle3
	ldp	x4, x5, [x1],#32
	ldp	x6, x7, [x1,#-16]
	b	Lmid
Lle3:	mov	x0, x15
	ret

	.align	4
Ltop:	ldp	x4, x5, [x1],#32
	ldp	x6, x7, [x1,#-16]
	subs	x8, x16, x8
	sbcs	x9, x17, x9
	stp	x8, x9, [x0],#32
	sbcs	x10, x12, x10
	sbcs	x11, x13, x11
	stp	x10, x11, [x0,#-16]
	csinc	x15, x15, x15, cs
Lmid:	sub	x2, x2, #1
	mul	x8, x4, x3
	umulh	x12, x4, x3
	mul	x9, x5, x3
	umulh	x13, x5, x3
	adds	x8, x8, x15
	mul	x10, x6, x3
	umulh	x14, x6, x3
	adcs	x9, x9, x12
	mul	x11, x7, x3
	umulh	x15, x7, x3
	adcs	x10, x10, x13
	ldp	x16, x17, [x0]
	adcs	x11, x11, x14
	ldp	x12, x13, [x0,#16]
	adc	x15, x15, xzr
	cbnz	x2, Ltop

	subs	x8, x16, x8
	sbcs	x9, x17, x9
	sbcs	x10, x12, x10
	sbcs	x11, x13, x11
	stp	x8, x9, [x0]
	stp	x10, x11, [x0,#16]
	csinc	x0, x15, x15, cs
	ret
	
