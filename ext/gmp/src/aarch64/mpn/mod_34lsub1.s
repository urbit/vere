




































































	.text
	.align	5
	.text
	.align	3
	.globl	___gmpn_mod_34lsub1 
	
___gmpn_mod_34lsub1:
	subs	x1, x1, #3
	mov	x8, #0
	b.lt	Lle2			

	ldp	x2, x3, [x0, #0]
	ldr	x4, [x0, #16]
	add	x0, x0, #24
	subs	x1, x1, #3
	b.lt	Lsum			
	cmn	x0, #0			

Ltop:	ldp	x5, x6, [x0, #0]
	ldr	x7, [x0, #16]
	add	x0, x0, #24
	sub	x1, x1, #3
	adcs	x2, x2, x5
	adcs	x3, x3, x6
	adcs	x4, x4, x7
	tbz	x1, #63, Ltop

	adc	x8, xzr, xzr		

Lsum:	cmn	x1, #2
	mov	x5, #0
	b.lo	1f
	ldr	x5, [x0], #8
1:	mov	x6, #0
	b.ls	1f
	ldr	x6, [x0], #8
1:	adds	x2, x2, x5
	adcs	x3, x3, x6
	adcs	x4, x4, xzr
	adc	x8, x8, xzr		

Lsum2:
	and	x0, x2, #0xffffffffffff
	add	x0, x0, x2, lsr #48
	add	x0, x0, x8

	lsl	x8, x3, #16
	and	x1, x8, #0xffffffffffff
	add	x0, x0, x1
	add	x0, x0, x3, lsr #32

	lsl	x8, x4, #32
	and	x1, x8, #0xffffffffffff
	add	x0, x0, x1
	add	x0, x0, x4, lsr #16
	ret

Lle2:	cmn	x1, #1
	b.ne	L1
	ldp	x2, x3, [x0]
	mov	x4, #0
	b	Lsum2
L1:	ldr	x2, [x0]
	and	x0, x2, #0xffffffffffff
	add	x0, x0, x2, lsr #48
	ret
	
