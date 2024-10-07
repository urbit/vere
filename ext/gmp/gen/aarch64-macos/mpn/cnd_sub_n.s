























































  
  
  
  




	.text
	.align	3
	.globl	___gmpn_cnd_sub_n 
	
___gmpn_cnd_sub_n:
	cmp	x0, #1
	sbc	x0, x0, x0

	cmp	xzr, xzr

	lsr	x18, x4, #2
	tbz	x4, #0, Lbx0

Lbx1:	ldr	x13, [x3]
	ldr	x11, [x2]
	bic	x7, x13, x0
	sbcs	x9, x11, x7
	str	x9, [x1]
	tbnz	x4, #1, Lb11

Lb01:	cbz	x18, Lrt
	ldp	x12, x13, [x3,#8]
	ldp	x10, x11, [x2,#8]
	sub	x2, x2, #8
	sub	x3, x3, #8
	sub	x1, x1, #24
	b	Lmid

Lb11:	ldp	x12, x13, [x3,#8]!
	ldp	x10, x11, [x2,#8]!
	sub	x1, x1, #8
	cbz	x18, Lend
	b	Ltop

Lbx0:	ldp	x12, x13, [x3]
	ldp	x10, x11, [x2]
	tbnz	x4, #1, Lb10

Lb00:	sub	x2, x2, #16
	sub	x3, x3, #16
	sub	x1, x1, #32
	b	Lmid

Lb10:	sub	x1, x1, #16
	cbz	x18, Lend

	.align	4
Ltop:	bic	x6, x12, x0
	bic	x7, x13, x0
	ldp	x12, x13, [x3,#16]
	sbcs	x8, x10, x6
	sbcs	x9, x11, x7
	ldp	x10, x11, [x2,#16]
	stp	x8, x9, [x1,#16]
Lmid:	bic	x6, x12, x0
	bic	x7, x13, x0
	ldp	x12, x13, [x3,#32]!
	sbcs	x8, x10, x6
	sbcs	x9, x11, x7
	ldp	x10, x11, [x2,#32]!
	stp	x8, x9, [x1,#32]!
	sub	x18, x18, #1
	cbnz	x18, Ltop

Lend:	bic	x6, x12, x0
	bic	x7, x13, x0
	sbcs	x8, x10, x6
	sbcs	x9, x11, x7
	stp	x8, x9, [x1,#16]
Lrt:	cset	x0, cc
	ret
	
