




























































 
  































	.text
	.align	4, 0x90
	.globl	___gmpn_mod_1_1p
	
	
___gmpn_mod_1_1p:

	
	push	%rbp
	push	%rbx
	mov	%rdx, %rbx
	mov	%rcx, %r8

	mov	-8(%rdi, %rsi, 8), %rax
	cmp	$3, %rsi
	jnc	Lfirst
	mov	-16(%rdi, %rsi, 8), %rbp
	jmp	Lreduce_two

Lfirst:
	
	mov	24(%r8), %r11
	mul	%r11
	mov	-24(%rdi, %rsi, 8), %rbp
	add	%rax, %rbp
	mov	-16(%rdi, %rsi, 8), %rax
	adc	%rdx, %rax
	sbb	%rcx, %rcx
	sub	$4, %rsi
	jc	Lreduce_three

	mov	%r11, %r10
	sub	%rbx, %r10

	.align	4, 0x90
Ltop:	and	%r11, %rcx
	lea	(%r10, %rbp), %r9
	mul	%r11
	add	%rbp, %rcx
	mov	(%rdi, %rsi, 8), %rbp
	cmovc	%r9, %rcx
	add	%rax, %rbp
	mov	%rcx, %rax
	adc	%rdx, %rax
	sbb	%rcx, %rcx
	sub	$1, %rsi
	jnc	Ltop

Lreduce_three:
	
	and	%rbx, %rcx
	sub	%rcx, %rax

Lreduce_two:
	mov	8(%r8), %ecx
	test	%ecx, %ecx
	jz	Lnormalized

	
	mulq	16(%r8)
	xor	%r9, %r9
	add	%rax, %rbp
	adc	%rdx, %r9
	mov	%r9, %rax

	

	shld	%cl, %rbp, %rax

	shl	%cl, %rbp
	jmp	Ludiv

Lnormalized:
	mov	%rax, %r9
	sub	%rbx, %r9
	cmovnc	%r9, %rax

Ludiv:
	lea	1(%rax), %r9
	mulq	(%r8)
	add	%rbp, %rax
	adc	%r9, %rdx
	imul	%rbx, %rdx
	sub	%rdx, %rbp
	cmp	%rbp, %rax
	lea	(%rbx, %rbp), %rax
	cmovnc	%rbp, %rax
	cmp	%rbx, %rax
	jnc	Lfix
Lok:	shr	%cl, %rax

	pop	%rbx
	pop	%rbp
	
	ret
Lfix:	sub	%rbx, %rax
	jmp	Lok
	

	.align	4, 0x90
	.globl	___gmpn_mod_1_1p_cps
	
	
___gmpn_mod_1_1p_cps:

	
	push	%rbp
	bsr	%rsi, %rcx
	push	%rbx
	mov	%rdi, %rbx
	push	%r12
	xor	$63, %ecx
	mov	%rsi, %r12
	mov	%ecx, %ebp
	sal	%cl, %r12
	mov	%r12, %rdi		
	

	
	call	___gmpn_invert_limb

	neg	%r12
	mov	%r12, %r8
	mov	%rax, (%rbx)		
	mov	%rbp, 8(%rbx)		
	imul	%rax, %r12
	mov	%r12, 24(%rbx)		
	mov	%ebp, %ecx
	test	%ecx, %ecx
	jz	Lz

	mov	$1, %edx

	shld	%cl, %rax, %rdx

	imul	%rdx, %r8
	shr	%cl, %r8
	mov	%r8, 16(%rbx)		
Lz:
	pop	%r12
	pop	%rbx
	pop	%rbp
	
	ret
	

