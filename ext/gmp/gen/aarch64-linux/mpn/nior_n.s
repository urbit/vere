





























































  
  
  






	.text
	.align	3
	.globl	__gmpn_nior_n 
	.type	__gmpn_nior_n,@function
__gmpn_nior_n:
	lsr	x18, x3, #2
	tbz	x3, #0, .Lbx0

.Lbx1:	ldr	x7, [x1]
	ldr	x11, [x2]
	orr	x15, x7, x11
	mvn	x15, x15
	str	x15, [x0],#8
	tbnz	x3, #1, .Lb11

.Lb01:	cbz	x18, .Lret
	ldp	x4, x5, [x1,#8]
	ldp	x8, x9, [x2,#8]
	sub	x1, x1, #8
	sub	x2, x2, #8
	b	.Lmid

.Lb11:	ldp	x6, x7, [x1,#8]
	ldp	x10, x11, [x2,#8]
	add	x1, x1, #8
	add	x2, x2, #8
	cbz	x18, .Lend
	b	.Ltop

.Lbx0:	tbnz	x3, #1, .Lb10

.Lb00:	ldp	x4, x5, [x1],#-16
	ldp	x8, x9, [x2],#-16
	b	.Lmid

.Lb10:	ldp	x6, x7, [x1]
	ldp	x10, x11, [x2]
	cbz	x18, .Lend

	.align	4
.Ltop:	ldp	x4, x5, [x1,#16]
	ldp	x8, x9, [x2,#16]
	orr	x12, x6, x10
	orr	x13, x7, x11
	mvn	x12, x12
	mvn	x13, x13
	stp	x12, x13, [x0],#16
.Lmid:	ldp	x6, x7, [x1,#32]!
	ldp	x10, x11, [x2,#32]!
	orr	x12, x4, x8
	orr	x13, x5, x9
	mvn	x12, x12
	mvn	x13, x13
	stp	x12, x13, [x0],#16
	sub	x18, x18, #1
	cbnz	x18, .Ltop

.Lend:	orr	x12, x6, x10
	orr	x13, x7, x11
	mvn	x12, x12
	mvn	x13, x13
	stp	x12, x13, [x0]
.Lret:	ret
	.size	__gmpn_nior_n,.-__gmpn_nior_n