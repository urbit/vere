




































































	
	
	
	
	


	
	
	








	.text
	.align	16, 0x90
	.globl	__gmpn_add_nc
	
	.def	__gmpn_add_nc
	.scl	2
	.type	32
	.endef
__gmpn_add_nc:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	56(%rsp), %r8	
	mov	%ecx, %eax
	shr	$2, %rcx
	and	$3, %eax
	bt	$0, %r8			
	jrcxz	Llt4

	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	dec	%rcx
	jmp	Lmid

	
	.align	16, 0x90
	.globl	__gmpn_add_n
	
	.def	__gmpn_add_n
	.scl	2
	.type	32
	.endef
__gmpn_add_n:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	%ecx, %eax
	shr	$2, %rcx
	and	$3, %eax
	jrcxz	Llt4

	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	dec	%rcx
	jmp	Lmid

Llt4:	dec	%eax
	mov	(%rsi), %r8
	jnz	L2
	adc	(%rdx), %r8
	mov	%r8, (%rdi)
	adc	%eax, %eax
	pop	%rsi
	pop	%rdi
	ret

L2:	dec	%eax
	mov	8(%rsi), %r9
	jnz	L3
	adc	(%rdx), %r8
	adc	8(%rdx), %r9
	mov	%r8, (%rdi)
	mov	%r9, 8(%rdi)
	adc	%eax, %eax
	pop	%rsi
	pop	%rdi
	ret

L3:	mov	16(%rsi), %r10
	adc	(%rdx), %r8
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	mov	%r8, (%rdi)
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	setc	%al
	pop	%rsi
	pop	%rdi
	ret

	.align	16, 0x90
Ltop:	adc	(%rdx), %r8
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	adc	24(%rdx), %r11
	mov	%r8, (%rdi)
	lea	32(%rsi), %rsi
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	dec	%rcx
	mov	%r11, 24(%rdi)
	lea	32(%rdx), %rdx
	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	lea	32(%rdi), %rdi
Lmid:	mov	16(%rsi), %r10
	mov	24(%rsi), %r11
	jnz	Ltop

Lend:	lea	32(%rsi), %rsi
	adc	(%rdx), %r8
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	adc	24(%rdx), %r11
	lea	32(%rdx), %rdx
	mov	%r8, (%rdi)
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	mov	%r11, 24(%rdi)
	lea	32(%rdi), %rdi

	inc	%eax
	dec	%eax
	jnz	Llt4
	adc	%eax, %eax
	pop	%rsi
	pop	%rdi
	ret
	
