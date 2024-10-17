























































	.text
	.align	3
	.globl	___gmpn_copyi 
	
___gmpn_copyi:
	cmp	x2, #3
	b.le	Lbc


	tbz	x0, #3, Lal2
	ldr	x4, [x1],#8
	sub	x2, x2, #1
	str	x4, [x0],#8

Lal2:	ldp	x4,x5, [x1],#16
	sub	x2, x2, #6
	tbnz	x2, #63, Lend

	.align	4
Ltop:	ldp	x6,x7, [x1],#32
	stp	x4,x5, [x0],#32
	ldp	x4,x5, [x1,#-16]
	stp	x6,x7, [x0,#-16]
	sub	x2, x2, #4
	tbz	x2, #63, Ltop

Lend:	stp	x4,x5, [x0],#16



Lbc:	tbz	x2, #1, Ltl1
	ldp	x4,x5, [x1],#16
	stp	x4,x5, [x0],#16
Ltl1:	tbz	x2, #0, Ltl2
	ldr	x4, [x1]
	str	x4, [x0]
Ltl2:	ret
	
