






















































  
  
  
  
  
  




	.text
	.align	3
	.globl	___gmpn_sub_nc 
	
___gmpn_sub_nc:
	cmp	xzr, x4
	b	Lent
	
	.text
	.align	3
	.globl	___gmpn_sub_n 
	
___gmpn_sub_n:
	cmp	xzr, xzr
Lent:	lsr	x18, x3, #2
	tbz	x3, #0, Lbx0

Lbx1:	ldr	x7, [x1]
	ldr	x11, [x2]
	sbcs	x13, x7, x11
	str	x13, [x0],#8
	tbnz	x3, #1, Lb11

Lb01:	cbz	x18, Lret
	ldp	x4, x5, [x1,#8]
	ldp	x8, x9, [x2,#8]
	sub	x1, x1, #8
	sub	x2, x2, #8
	b	Lmid

Lb11:	ldp	x6, x7, [x1,#8]
	ldp	x10, x11, [x2,#8]
	add	x1, x1, #8
	add	x2, x2, #8
	cbz	x18, Lend
	b	Ltop

Lbx0:	tbnz	x3, #1, Lb10

Lb00:	ldp	x4, x5, [x1]
	ldp	x8, x9, [x2]
	sub	x1, x1, #16
	sub	x2, x2, #16
	b	Lmid

Lb10:	ldp	x6, x7, [x1]
	ldp	x10, x11, [x2]
	cbz	x18, Lend

	.align	4
Ltop:	ldp	x4, x5, [x1,#16]
	ldp	x8, x9, [x2,#16]
	sbcs	x12, x6, x10
	sbcs	x13, x7, x11
	stp	x12, x13, [x0],#16
Lmid:	ldp	x6, x7, [x1,#32]!
	ldp	x10, x11, [x2,#32]!
	sbcs	x12, x4, x8
	sbcs	x13, x5, x9
	stp	x12, x13, [x0],#16
	sub	x18, x18, #1
	cbnz	x18, Ltop

Lend:	sbcs	x12, x6, x10
	sbcs	x13, x7, x11
	stp	x12, x13, [x0]
Lret:	cset	x0, cc
	ret
	
