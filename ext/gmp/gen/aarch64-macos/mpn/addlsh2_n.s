









































































  
  
  
  
  




	.text
	.align	3
	.globl	___gmpn_addlsh2_n 
	
___gmpn_addlsh2_n:
	lsr	x6, x3, #2
	tbz	x3, #0, Lbx0

Lbx1:	ldr	x5, [x1]
	tbnz	x3, #1, Lb11

Lb01:	ldr	x11, [x2]
	cbz	x6, L1
	ldp	x8, x9, [x2,#8]
	lsl	x13, x11, #2
	adds	x15, x13, x5
	str	x15, [x0],#8
	sub	x1, x1, #24
	sub	x2, x2, #8
	b	Lmid

L1:	lsl	x13, x11, #2
	adds	x15, x13, x5
	str	x15, [x0]
	lsr	x0, x11, 62
	adc	x0, x0, xzr
	ret

Lb11:	ldr	x9, [x2]
	ldp	x10, x11, [x2,#8]!
	lsl	x13, x9, #2
	adds	x17, x13, x5
	str	x17, [x0],#8
	sub	x1, x1, #8
	cbz	x6, Lend
	b	Ltop

Lbx0:	tbnz	x3, #1, Lb10

Lb00:	adds	x11, xzr, xzr
	ldp	x8, x9, [x2],#-16
	sub	x1, x1, #32
	b	Lmid

Lb10:	adds	x9, xzr, xzr
	ldp	x10, x11, [x2]
	sub	x1, x1, #16
	cbz	x6, Lend

	.align	4
Ltop:	ldp	x4, x5, [x1,#16]
	extr	x12, x10, x9, #62
	ldp	x8, x9, [x2,#16]
	extr	x13, x11, x10, #62
	adcs	x14, x12, x4
	adcs	x15, x13, x5
	stp	x14, x15, [x0],#16
Lmid:	ldp	x4, x5, [x1,#32]!
	extr	x12, x8, x11, #62
	ldp	x10, x11, [x2,#32]!
	extr	x13, x9, x8, #62
	adcs	x16, x12, x4
	adcs	x17, x13, x5
	stp	x16, x17, [x0],#16
	sub	x6, x6, #1
	cbnz	x6, Ltop

Lend:	ldp	x4, x5, [x1,#16]
	extr	x12, x10, x9, #62
	extr	x13, x11, x10, #62
	adcs	x14, x12, x4
	adcs	x15, x13, x5
	stp	x14, x15, [x0]
	lsr	x0, x11, 62
	adc	x0, x0, xzr
	ret
	

