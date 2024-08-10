



















































	.text
	.align	3
	.globl	___gmpn_com 
	
___gmpn_com:
	cmp	x2, #3
	b.le	Lbc


	tbz	x0, #3, Lal2
	ld1	{v22.1d}, [x1], #8
	sub	x2, x2, #1
	mvn	v22.8b, v22.8b
	st1	{v22.1d}, [x0], #8

Lal2:	ld1	{v26.2d}, [x1], #16
	subs	x2, x2, #6
	b.lt	Lend

	.align	4
Ltop:	ld1	{v22.2d}, [x1], #16
	mvn	v26.16b, v26.16b
	st1	{v26.2d}, [x0], #16
	ld1	{v26.2d}, [x1], #16
	mvn	v22.16b, v22.16b
	st1	{v22.2d}, [x0], #16
	subs	x2, x2, #4
	b.ge	Ltop

Lend:	mvn	v26.16b, v26.16b
	st1	{v26.2d}, [x0], #16



Lbc:	tbz	x2, #1, Ltl1
	ld1	{v22.2d}, [x1], #16
	mvn	v22.16b, v22.16b
	st1	{v22.2d}, [x0], #16
Ltl1:	tbz	x2, #0, Ltl2
	ld1	{v22.1d}, [x1]
	mvn	v22.8b, v22.8b
	st1	{v22.1d}, [x0]
Ltl2:	ret
	
