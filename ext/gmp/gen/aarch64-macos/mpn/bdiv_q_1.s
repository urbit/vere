
























































		
		





	.text
	.align	3
	.globl	___gmpn_bdiv_q_1 
	
___gmpn_bdiv_q_1:

	rbit	x6, x3
	clz	x5, x6
	lsr	x3, x3, x5

	adrp	x7, ___gmp_binvert_limb_table@GOTPAGE
	ubfx	x6, x3, 1, 7
	ldr	x7, [x7, ___gmp_binvert_limb_table@GOTPAGEOFF]
	ldrb	w6, [x7, x6]
	ubfiz	x7, x6, 1, 8
	umull	x6, w6, w6
	msub	x6, x6, x3, x7
	lsl	x7, x6, 1
	mul	x6, x6, x6
	msub	x6, x6, x3, x7
	lsl	x7, x6, 1
	mul	x6, x6, x6
	msub	x4, x6, x3, x7

	b	___gmpn_pi1_bdiv_q_1
	

	.text
	.align	3
	.globl	___gmpn_pi1_bdiv_q_1 
	
___gmpn_pi1_bdiv_q_1:
	sub	x2, x2, #1
	subs	x6, x6, x6		
	ldr	x9, [x1],#8
	cbz	x5, Lnorm

Lunorm:
	lsr	x12, x9, x5
	cbz	x2, Leu1
	sub	x8, xzr, x5

Ltpu:	ldr	x9, [x1],#8
	lsl	x7, x9, x8
	orr	x7, x7, x12
	sbcs	x6, x7, x6
	mul	x7, x6, x4
	str	x7, [x0],#8
	lsr	x12, x9, x5
	umulh	x6, x7, x3
	sub	x2, x2, #1
	cbnz	x2, Ltpu

Leu1:	sbcs	x6, x12, x6
	mul	x6, x6, x4
	str	x6, [x0]
	ret

Lnorm:
	mul	x5, x9, x4
	str	x5, [x0],#8
	cbz	x2, Len1

Ltpn:	ldr	x9, [x1],#8
	umulh	x5, x5, x3
	sbcs	x5, x9, x5
	mul	x5, x5, x4
	str	x5, [x0],#8
	sub	x2, x2, #1
	cbnz	x2, Ltpn

Len1:	ret
	
