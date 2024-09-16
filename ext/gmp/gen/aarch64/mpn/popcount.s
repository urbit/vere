



































































	.text
	.align	3
	.globl	___gmpn_popcount 
	
___gmpn_popcount:

	mov	x11, #0x1fff
	cmp	x1, x11
	b.hi	Lgt8k

Llt8k:
	movi	v4.16b, #0			
	movi	v5.16b, #0			

	tbz	x1, #0, Lxx0
	sub	x1, x1, #1
	ld1	{v0.1d}, [x0], #8		
	cnt	v6.16b, v0.16b
	uadalp	v4.8h,  v6.16b			

Lxx0:	tbz	x1, #1, Lx00
	sub	x1, x1, #2
	ld1	{v0.2d}, [x0], #16		
	cnt	v6.16b, v0.16b
	uadalp	v4.8h,  v6.16b

Lx00:	tbz	x1, #2, L000
	subs	x1, x1, #4
	ld1	{v0.2d,v1.2d}, [x0], #32	
	b.ls	Lsum

Lgt4:	ld1	{v2.2d,v3.2d}, [x0], #32	
	sub	x1, x1, #4
	cnt	v6.16b, v0.16b
	cnt	v7.16b, v1.16b
	b	Lmid

L000:	subs	x1, x1, #8
	b.lo	Le0

Lchu:	ld1	{v2.2d,v3.2d}, [x0], #32	
	ld1	{v0.2d,v1.2d}, [x0], #32	
	cnt	v6.16b, v2.16b
	cnt	v7.16b, v3.16b
	subs	x1, x1, #8
	b.lo	Lend

Ltop:	ld1	{v2.2d,v3.2d}, [x0], #32	
	uadalp	v4.8h,  v6.16b
	cnt	v6.16b, v0.16b
	uadalp	v5.8h,  v7.16b
	cnt	v7.16b, v1.16b
Lmid:	ld1	{v0.2d,v1.2d}, [x0], #32	
	subs	x1, x1, #8
	uadalp	v4.8h,  v6.16b
	cnt	v6.16b, v2.16b
	uadalp	v5.8h,  v7.16b
	cnt	v7.16b, v3.16b
	b.hs	Ltop

Lend:	uadalp	v4.8h,  v6.16b
	uadalp	v5.8h,  v7.16b
Lsum:	cnt	v6.16b, v0.16b
	cnt	v7.16b, v1.16b
	uadalp	v4.8h,  v6.16b
	uadalp	v5.8h,  v7.16b
	add	v4.8h, v4.8h, v5.8h
					
Le0:	uaddlp	v4.4s,  v4.8h		
	uaddlp	v4.2d,  v4.4s		
	mov	x0, v4.d[0]
	mov	x1, v4.d[1]
	add	x0, x0, x1
	ret


			
Lgt8k:
	mov	x8, x30
	mov	x7, x1			
	mov	x4, #0			
	mov	x9, #0x1ff0*8	
	mov	x10, #0x1ff0		

1:	add	x5, x0, x9		
	mov	x1, #0x1ff0-8		
	movi	v4.16b, #0		
	movi	v5.16b, #0		
	bl	Lchu			
	add	x4, x4, x0
	mov	x0, x5			
	sub	x7, x7, x10
	cmp	x7, x11
	b.hi	1b

	mov	x1, x7			
	bl	Llt8k
	add	x0, x4, x0
	mov	x30, x8
	ret
	
