




































































	.text
	.align	3
	.globl	___gmpn_hamdist 
	
___gmpn_hamdist:

	mov	x11, #0x1fff
	cmp	x2, x11
	b.hi	Lgt8k

Llt8k:
	movi	v4.16b, #0			
	movi	v5.16b, #0			

	tbz	x2, #0, Lxx0
	sub	x2, x2, #1
	ld1	{v0.1d}, [x0], #8		
	ld1	{v16.1d}, [x1], #8		
	eor	v0.16b, v0.16b, v16.16b
	cnt	v6.16b, v0.16b
	uadalp	v4.8h,  v6.16b			

Lxx0:	tbz	x2, #1, Lx00
	sub	x2, x2, #2
	ld1	{v0.2d}, [x0], #16		
	ld1	{v16.2d}, [x1], #16		
	eor	v0.16b, v0.16b, v16.16b
	cnt	v6.16b, v0.16b
	uadalp	v4.8h,  v6.16b

Lx00:	tbz	x2, #2, L000
	subs	x2, x2, #4
	ld1	{v0.2d,v1.2d}, [x0], #32	
	ld1	{v16.2d,v17.2d}, [x1], #32	
	b.ls	Lsum

Lgt4:	ld1	{v2.2d,v3.2d}, [x0], #32	
	ld1	{v18.2d,v19.2d}, [x1], #32	
	eor	v0.16b, v0.16b, v16.16b
	eor	v1.16b, v1.16b, v17.16b
	sub	x2, x2, #4
	cnt	v6.16b, v0.16b
	cnt	v7.16b, v1.16b
	b	Lmid

L000:	subs	x2, x2, #8
	b.lo	Le0

Lchu:	ld1	{v2.2d,v3.2d}, [x0], #32	
	ld1	{v0.2d,v1.2d}, [x0], #32	
	ld1	{v18.2d,v19.2d}, [x1], #32	
	ld1	{v16.2d,v17.2d}, [x1], #32	
	eor	v2.16b, v2.16b, v18.16b
	eor	v3.16b, v3.16b, v19.16b
	cnt	v6.16b, v2.16b
	cnt	v7.16b, v3.16b
	subs	x2, x2, #8
	b.lo	Lend

Ltop:	ld1	{v2.2d,v3.2d}, [x0], #32	
	ld1	{v18.2d,v19.2d}, [x1], #32	
	eor	v0.16b, v0.16b, v16.16b
	eor	v1.16b, v1.16b, v17.16b
	uadalp	v4.8h,  v6.16b
	cnt	v6.16b, v0.16b
	uadalp	v5.8h,  v7.16b
	cnt	v7.16b, v1.16b
Lmid:	ld1	{v0.2d,v1.2d}, [x0], #32	
	ld1	{v16.2d,v17.2d}, [x1], #32	
	eor	v2.16b, v2.16b, v18.16b
	eor	v3.16b, v3.16b, v19.16b
	subs	x2, x2, #8
	uadalp	v4.8h,  v6.16b
	cnt	v6.16b, v2.16b
	uadalp	v5.8h,  v7.16b
	cnt	v7.16b, v3.16b
	b.hs	Ltop

Lend:	uadalp	v4.8h,  v6.16b
	uadalp	v5.8h,  v7.16b
Lsum:	eor	v0.16b, v0.16b, v16.16b
	eor	v1.16b, v1.16b, v17.16b
	cnt	v6.16b, v0.16b
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
	mov	x7, x2			
	mov	x4, #0			
	mov	x9, #0x1ff0*8	
	mov	x10, #0x1ff0		

1:	add	x5, x0, x9		
	add	x6, x1, x9		
	mov	x2, #0x1ff0-8		
	movi	v4.16b, #0		
	movi	v5.16b, #0		
	bl	Lchu			
	add	x4, x4, x0
	mov	x0, x5			
	mov	x1, x6			
	sub	x7, x7, x10
	cmp	x7, x11
	b.hi	1b

	mov	x2, x7			
	bl	Llt8k
	add	x0, x4, x0
	mov	x30, x8
	ret
	
