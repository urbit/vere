















































	
	
	
	





































	.text
	.align	4, 0x90
	.globl	___gmpn_addlsh2_nc
	
	
___gmpn_addlsh2_nc:

	

	push	%rbp
	mov	%r8, %rax
	neg	%rax			
	xor	%ebp, %ebp	
	mov	(%rdx), %r8
	shrd	$62, %r8, %rbp
	mov	%ecx, %r9d
	and	$3, %r9d
	je	Lb00
	cmp	$2, %r9d
	jc	Lb01
	je	Lb10
	jmp	Lb11
	

	.align	4, 0x90
	.globl	___gmpn_addlsh2_n
	
	
___gmpn_addlsh2_n:

	
	push	%rbp
	xor	%ebp, %ebp	
	mov	(%rdx), %r8
	shrd	$62, %r8, %rbp
	mov	%ecx, %eax
	and	$3, %eax
	je	Lb00
	cmp	$2, %eax
	jc	Lb01
	je	Lb10

Lb11:	mov	8(%rdx), %r9
	shrd	$62, %r9, %r8
	mov	16(%rdx), %r10
	shrd	$62, %r10, %r9
	add	%eax, %eax	
	adc	(%rsi), %rbp
	adc	8(%rsi), %r8
	adc	16(%rsi), %r9
	mov	%rbp, (%rdi)
	mov	%r8, 8(%rdi)
	mov	%r9, 16(%rdi)
	mov	%r10, %rbp
	lea	24(%rsi), %rsi
	lea	24(%rdx), %rdx
	lea	24(%rdi), %rdi
	sbb	%eax, %eax	
	sub	$3, %rcx
	ja	Ltop
	jmp	Lend

Lb01:	add	%eax, %eax	
	adc	(%rsi), %rbp
	mov	%rbp, (%rdi)
	mov	%r8, %rbp
	lea	8(%rsi), %rsi
	lea	8(%rdx), %rdx
	lea	8(%rdi), %rdi
	sbb	%eax, %eax	
	sub	$1, %rcx
	ja	Ltop
	jmp	Lend

Lb10:	mov	8(%rdx), %r9
	shrd	$62, %r9, %r8
	add	%eax, %eax	
	adc	(%rsi), %rbp
	adc	8(%rsi), %r8
	mov	%rbp, (%rdi)
	mov	%r8, 8(%rdi)
	mov	%r9, %rbp
	lea	16(%rsi), %rsi
	lea	16(%rdx), %rdx
	lea	16(%rdi), %rdi
	sbb	%eax, %eax	
	sub	$2, %rcx
	ja	Ltop
	jmp	Lend

	.align	4, 0x90
Ltop:	mov	(%rdx), %r8
	shrd	$62, %r8, %rbp
Lb00:	mov	8(%rdx), %r9
	shrd	$62, %r9, %r8
	mov	16(%rdx), %r10
	shrd	$62, %r10, %r9
	mov	24(%rdx), %r11
	shrd	$62, %r11, %r10
	lea	32(%rdx), %rdx
	add	%eax, %eax	
	adc	(%rsi), %rbp
	adc	8(%rsi), %r8
	adc	16(%rsi), %r9
	adc	24(%rsi), %r10
	lea	32(%rsi), %rsi
	mov	%rbp, (%rdi)
	mov	%r8, 8(%rdi)
	mov	%r9, 16(%rdi)
	mov	%r10, 24(%rdi)
	mov	%r11, %rbp
	lea	32(%rdi), %rdi
	sbb	%eax, %eax	
	sub	$4, %rcx
	jnz	Ltop

Lend:	shr	$62, %rbp
	add	%eax, %eax	
	adc	$0, %rbp
	mov	%rbp, %rax
	pop	%rbp
	
	ret
	

