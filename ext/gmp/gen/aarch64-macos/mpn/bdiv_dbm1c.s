



















































	.text
	.align	4
	.text
	.align	3
	.globl	___gmpn_bdiv_dbm1c 
	
___gmpn_bdiv_dbm1c:
	ldr	x5, [x1], #8
	ands	x6, x2, #3
	b.eq	Lfi0
	cmp	x6, #2
	b.cc	Lfi1
	b.eq	Lfi2

Lfi3:	mul	x12, x5, x3
	umulh	x13, x5, x3
	ldr	x5, [x1], #8
	b	Llo3

Lfi0:	mul	x10, x5, x3
	umulh	x11, x5, x3
	ldr	x5, [x1], #8
	b	Llo0

Lfi1:	subs	x2, x2, #1
	mul	x12, x5, x3
	umulh	x13, x5, x3
	b.ls	Lwd1
	ldr	x5, [x1], #8
	b	Llo1

Lfi2:	mul	x10, x5, x3
	umulh	x11, x5, x3
	ldr	x5, [x1], #8
	b	Llo2

Ltop:	ldr	x5, [x1], #8
	subs	x4, x4, x10
	str	x4, [x0], #8
	sbc	x4, x4, x11
Llo1:	mul	x10, x5, x3
	umulh	x11, x5, x3
	ldr	x5, [x1], #8
	subs	x4, x4, x12
	str	x4, [x0], #8
	sbc	x4, x4, x13
Llo0:	mul	x12, x5, x3
	umulh	x13, x5, x3
	ldr	x5, [x1], #8
	subs	x4, x4, x10
	str	x4, [x0], #8
	sbc	x4, x4, x11
Llo3:	mul	x10, x5, x3
	umulh	x11, x5, x3
	ldr	x5, [x1], #8
	subs	x4, x4, x12
	str	x4, [x0], #8
	sbc	x4, x4, x13
Llo2:	subs	x2, x2, #4
	mul	x12, x5, x3
	umulh	x13, x5, x3
	b.hi	Ltop

Lwd2:	subs	x4, x4, x10
	str	x4, [x0], #8
	sbc	x4, x4, x11
Lwd1:	subs	x4, x4, x12
	str	x4, [x0]
	sbc	x0, x4, x13
	ret
	
