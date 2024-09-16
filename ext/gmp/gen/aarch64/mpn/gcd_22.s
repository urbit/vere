
































































	.text
	.align	3
	.globl	___gmpn_gcd_22 
	
___gmpn_gcd_22:

	.align	4
Ltop:	subs	x5, x1, x3		
	cbz	x5, Llowz
	sbcs	x6, x0, x2		

	rbit	x7, x5			

	cneg	x5, x5, cc		
	cinv	x6, x6, cc		
Lbck:	csel	x3, x3, x1, cs		
	csel	x2, x2, x0, cs		

	clz	x7, x7		
	sub	x8, xzr, x7		

	lsr	x1, x5, x7		
	lsl	x14, x6, x8		
	lsr	x0, x6, x7		
	orr	x1, x1, x14		

	orr	x11, x0, x2
	cbnz	x11, Ltop


	subs	x4, x1, x3		
	b.eq	Lend1			

	.align	4
Ltop1:rbit	x12, x4			
	clz	x12, x12		
	csneg	x4, x4, x4, cs		
	csel	x1, x3, x1, cs		
	lsr	x3, x4, x12		
	subs	x4, x1, x3		
	b.ne	Ltop1			
Lend1:mov	x0, x1
	mov	x1, #0
	ret

Llowz:
	
	
	subs	x5, x0, x2
	b.eq	Lend
	mov	x6, #0
	rbit	x7, x5			
	cneg	x5, x5, cc		
	b	Lbck			

Lend:	mov	x0, x3
	mov	x1, x2
	ret
	
