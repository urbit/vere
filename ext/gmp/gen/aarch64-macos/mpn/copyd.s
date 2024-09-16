


















































	.text
	.align	3
	.globl	___gmpn_copyd 
	
___gmpn_copyd:
	add	x0, x0, x2, lsl #3
	add	x1, x1, x2, lsl #3

	cmp	x2, #3
	b.le	Lbc


	tbz	x0, #3, Lal2
	sub	x1, x1, #8
	ld1	{v22.1d}, [x1]
	sub	x2, x2, #1
	sub	x0, x0, #8
	st1	{v22.1d}, [x0]

Lal2:	sub	x1, x1, #16
	ld1	{v26.2d}, [x1]
	sub	x2, x2, #6
	sub	x0, x0, #16			
	tbnz	x2, #63, Lend

	sub	x1, x1, #16			
	mov	x12, #-16

	.align	4
Ltop:	ld1	{v22.2d}, [x1], x12
	st1	{v26.2d}, [x0], x12
	ld1	{v26.2d}, [x1], x12
	st1	{v22.2d}, [x0], x12
	sub	x2, x2, #4
	tbz	x2, #63, Ltop

	add	x1, x1, #16			

Lend:	st1	{v26.2d}, [x0]



Lbc:	tbz	x2, #1, Ltl1
	sub	x1, x1, #16
	ld1	{v22.2d}, [x1]
	sub	x0, x0, #16
	st1	{v22.2d}, [x0]
Ltl1:	tbz	x2, #0, Ltl2
	sub	x1, x1, #8
	ld1	{v22.1d}, [x1]
	sub	x0, x0, #8
	st1	{v22.1d}, [x0]
Ltl2:	ret
	
