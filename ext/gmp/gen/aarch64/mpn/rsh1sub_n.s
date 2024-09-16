






















































  
  
  
  




	.text
	.align	3
	.globl	___gmpn_rsh1sub_n 
	
___gmpn_rsh1sub_n:
	lsr	x18, x3, #2

	tbz	x3, #0, Lbx0

Lbx1:	ldr	x5, [x1],#8
	ldr	x9, [x2],#8
	tbnz	x3, #1, Lb11

Lb01:	subs	x13, x5, x9
	and	x10, x13, #1
	cbz	x18, L1
	ldp	x4, x5, [x1],#48
	ldp	x8, x9, [x2],#48
	sbcs	x14, x4, x8
	sbcs	x15, x5, x9
	ldp	x4, x5, [x1,#-32]
	ldp	x8, x9, [x2,#-32]
	extr	x17, x14, x13, #1
	sbcs	x12, x4, x8
	sbcs	x13, x5, x9
	str	x17, [x0], #24
	sub	x18, x18, #1
	cbz	x18, Lend
	b	Ltop

L1:	cset	x14, cc
	extr	x17, x14, x13, #1
	str	x17, [x0]
	mov	x0, x10
	ret

Lb11:	subs	x15, x5, x9
	and	x10, x15, #1

	ldp	x4, x5, [x1],#32
	ldp	x8, x9, [x2],#32
	sbcs	x12, x4, x8
	sbcs	x13, x5, x9
	cbz	x18, L3
	ldp	x4, x5, [x1,#-16]
	ldp	x8, x9, [x2,#-16]
	extr	x17, x12, x15, #1
	sbcs	x14, x4, x8
	sbcs	x15, x5, x9
	str	x17, [x0], #8
	b	Lmid

L3:	extr	x17, x12, x15, #1
	str	x17, [x0], #8
	b	L2

Lbx0:	tbz	x3, #1, Lb00

Lb10:	ldp	x4, x5, [x1],#32
	ldp	x8, x9, [x2],#32
	subs	x12, x4, x8
	sbcs	x13, x5, x9
	and	x10, x12, #1
	cbz	x18, L2
	ldp	x4, x5, [x1,#-16]
	ldp	x8, x9, [x2,#-16]
	sbcs	x14, x4, x8
	sbcs	x15, x5, x9
	b	Lmid

Lb00:	ldp	x4, x5, [x1],#48
	ldp	x8, x9, [x2],#48
	subs	x14, x4, x8
	sbcs	x15, x5, x9
	and	x10, x14, #1
	ldp	x4, x5, [x1,#-32]
	ldp	x8, x9, [x2,#-32]
	sbcs	x12, x4, x8
	sbcs	x13, x5, x9
	add	x0, x0, #16
	sub	x18, x18, #1
	cbz	x18, Lend

	.align	4
Ltop:	ldp	x4, x5, [x1,#-16]
	ldp	x8, x9, [x2,#-16]
	extr	x16, x15, x14, #1
	extr	x17, x12, x15, #1
	sbcs	x14, x4, x8
	sbcs	x15, x5, x9
	stp	x16, x17, [x0,#-16]
Lmid:	ldp	x4, x5, [x1],#32
	ldp	x8, x9, [x2],#32
	extr	x16, x13, x12, #1
	extr	x17, x14, x13, #1
	sbcs	x12, x4, x8
	sbcs	x13, x5, x9
	stp	x16, x17, [x0],#32
	sub	x18, x18, #1
	cbnz	x18, Ltop

Lend:	extr	x16, x15, x14, #1
	extr	x17, x12, x15, #1
	stp	x16, x17, [x0,#-16]
L2:	cset	x14, cc
	extr	x16, x13, x12, #1
	extr	x17, x14, x13, #1
	stp	x16, x17, [x0]

Lret:	mov	x0, x10
	ret
	
