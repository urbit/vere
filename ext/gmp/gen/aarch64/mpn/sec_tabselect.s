
































































	.text
	.align	3
	.globl	___gmpn_sec_tabselect 
	
___gmpn_sec_tabselect:
	dup	v7.2d, x4			

	mov	x10, #1
	dup	v6.2d, x10			

	subs	x6, x2, #4
	b.mi	Louter_end

Louter_top:
	mov	x5, x3
	mov	x12, x1				
	movi	v5.16b, #0			
	movi	v2.16b, #0
	movi	v3.16b, #0
	.align	4
Ltp4:	cmeq	v4.2d, v5.2d, v7.2d		
	ld1	{v0.2d,v1.2d}, [x1]
	add	v5.2d, v5.2d, v6.2d
	bit	v2.16b, v0.16b, v4.16b
	bit	v3.16b, v1.16b, v4.16b
	add	x1, x1, x2, lsl #3
	sub	x5, x5, #1
	cbnz	x5, Ltp4
	st1	{v2.2d,v3.2d}, [x0], #32
	add	x1, x12, #32			
	subs	x6, x6, #4
	b.pl	Louter_top
Louter_end:

	tbz	x2, #1, Lb0x
	mov	x5, x3
	mov	x12, x1
	movi	v5.16b, #0			
	movi	v2.16b, #0
	.align	4
Ltp2:	cmeq	v4.2d, v5.2d, v7.2d
	ld1	{v0.2d}, [x1]
	add	v5.2d, v5.2d, v6.2d
	bit	v2.16b, v0.16b, v4.16b
	add	x1, x1, x2, lsl #3
	sub	x5, x5, #1
	cbnz	x5, Ltp2
	st1	{v2.2d}, [x0], #16
	add	x1, x12, #16

Lb0x:	tbz	x2, #0, Lb00
	mov	x5, x3
	mov	x12, x1
	movi	v5.16b, #0			
	movi	v2.16b, #0
	.align	4
Ltp1:	cmeq	v4.2d, v5.2d, v7.2d
	ld1	{v0.1d}, [x1]
	add	v5.2d, v5.2d, v6.2d		
	bit	v2.8b, v0.8b, v4.8b
	add	x1, x1, x2, lsl #3
	sub	x5, x5, #1
	cbnz	x5, Ltp1
	st1	{v2.1d}, [x0], #8
	add	x1, x12, #8

Lb00:	ret
	
