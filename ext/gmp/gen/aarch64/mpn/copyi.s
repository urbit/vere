



















































	.text
	.align	3
	.globl	___gmpn_copyi 
	
___gmpn_copyi:
	cmp	x2, #3
	b.le	Lbc


	tbz	x0, #3, Lal2
	ld1	{v22.1d}, [x1], #8
	sub	x2, x2, #1
	st1	{v22.1d}, [x0], #8

Lal2:	ld1	{v26.2d}, [x1], #16
	sub	x2, x2, #6
	tbnz	x2, #63, Lend

	.align	4
Ltop:	ld1	{v22.2d}, [x1], #16
	st1	{v26.2d}, [x0], #16
	ld1	{v26.2d}, [x1], #16
	st1	{v22.2d}, [x0], #16
	sub	x2, x2, #4
	tbz	x2, #63, Ltop

Lend:	st1	{v26.2d}, [x0], #16



Lbc:	tbz	x2, #1, Ltl1
	ld1	{v22.2d}, [x1], #16
	st1	{v22.2d}, [x0], #16
Ltl1:	tbz	x2, #0, Ltl2
	ld1	{v22.1d}, [x1]
	st1	{v22.1d}, [x0]
Ltl2:	ret
	
