


























































	.text
	.align	4
	.text
	.align	3
	.globl	__gmpn_gcd_11 
	.type	__gmpn_gcd_11,@function
__gmpn_gcd_11:
	subs	x3, x0, x1		
	b.eq	.Lend			

	.align	4
.Ltop:	rbit	x12, x3			
	clz	x12, x12		
	csneg	x3, x3, x3, cs		
	csel	x0, x1, x0, cs		
	lsr	x1, x3, x12		
	subs	x3, x0, x1		
	b.ne	.Ltop			

.Lend:	ret
	.size	__gmpn_gcd_11,.-__gmpn_gcd_11