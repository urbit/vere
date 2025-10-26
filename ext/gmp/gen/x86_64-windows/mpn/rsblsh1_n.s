






































































  
  
  







	.text
	.align	16, 0x90
	.globl	__gmpn_rsblsh1_n
	
	.def	__gmpn_rsblsh1_n
	.scl	2
	.type	32
	.endef
__gmpn_rsblsh1_n:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	push	%rbp

	mov	(%rdx), %r8
	mov	%ecx, %eax
	lea	(%rdi,%rcx,8), %rdi
	lea	(%rsi,%rcx,8), %rsi
	lea	(%rdx,%rcx,8), %rdx
	neg	%rcx
	xor	%ebp, %ebp
	and	$3, %eax
	je	Lb00
	cmp	$2, %eax
	jc	Lb01
	je	Lb10

Lb11:	add	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	mov	16(%rdx,%rcx,8), %r10
	adc	%r10, %r10
	sbb	%eax, %eax	
	sub	(%rsi,%rcx,8), %r8
	sbb	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
	sbb	16(%rsi,%rcx,8), %r10
	mov	%r10, 16(%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	add	$3, %rcx
	jmp	Lent

Lb10:	add	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	sbb	%eax, %eax	
	sub	(%rsi,%rcx,8), %r8
	sbb	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	add	$2, %rcx
	jmp	Lent

Lb01:	add	%r8, %r8
	sbb	%eax, %eax	
	sub	(%rsi,%rcx,8), %r8
	mov	%r8, (%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	inc	%rcx
Lent:	jns	Lend

	.align	16, 0x90
Ltop:	add	%eax, %eax	

	mov	(%rdx,%rcx,8), %r8
Lb00:	adc	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	mov	16(%rdx,%rcx,8), %r10
	adc	%r10, %r10
	mov	24(%rdx,%rcx,8), %r11
	adc	%r11, %r11

	sbb	%eax, %eax	
	add	%ebp, %ebp	

	sbb	(%rsi,%rcx,8), %r8
	nop				
	sbb	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
	sbb	16(%rsi,%rcx,8), %r10
	sbb	24(%rsi,%rcx,8), %r11
	mov	%r10, 16(%rdi,%rcx,8)
	mov	%r11, 24(%rdi,%rcx,8)

	sbb	%ebp, %ebp	
	add	$4, %rcx
	js	Ltop

Lend:


	sub	%eax, %ebp
	movslq	%ebp, %rax

	pop	%rbp
	pop	%rsi
	pop	%rdi
	ret
	
