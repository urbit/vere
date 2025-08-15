




























































	
	
	
	








	.text

	.align	16, 0x90
	.globl	__gmpn_rsh1add_nc
	.type	__gmpn_rsh1add_nc,@function
	
__gmpn_rsh1add_nc:

	

	push	%rbx
	push	%rbp

	neg	%r8			
	mov	(%rsi), %rbp
	adc	(%rdx), %rbp

	jmp	.Lent
	.size	__gmpn_rsh1add_nc,.-__gmpn_rsh1add_nc

	.align	16, 0x90
	.globl	__gmpn_rsh1add_n
	.type	__gmpn_rsh1add_n,@function
	
__gmpn_rsh1add_n:

	
	push	%rbx
	push	%rbp

	mov	(%rsi), %rbp
	add	(%rdx), %rbp
.Lent:
	sbb	%ebx, %ebx	
	mov	%ebp, %eax
	and	$1, %eax		

	mov	%ecx, %r11d
	and	$3, %r11d

	cmp	$1, %r11d
	je	.Ldo			

.Ln1:	cmp	$2, %r11d
	jne	.Ln2			
	add	%ebx, %ebx	
	mov	8(%rsi), %r10
	adc	8(%rdx), %r10
	lea	8(%rsi), %rsi
	lea	8(%rdx), %rdx
	lea	8(%rdi), %rdi
	sbb	%ebx, %ebx	

	shrd	$1, %r10, %rbp
	mov	%rbp, -8(%rdi)
	jmp	.Lcj1

.Ln2:	cmp	$3, %r11d
	jne	.Ln3			
	add	%ebx, %ebx	
	mov	8(%rsi), %r9
	mov	16(%rsi), %r10
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	lea	16(%rsi), %rsi
	lea	16(%rdx), %rdx
	lea	16(%rdi), %rdi
	sbb	%ebx, %ebx	

	shrd	$1, %r9, %rbp
	mov	%rbp, -16(%rdi)
	jmp	.Lcj2

.Ln3:	dec	%rcx			
	add	%ebx, %ebx	
	mov	8(%rsi), %r8
	mov	16(%rsi), %r9
	adc	8(%rdx), %r8
	adc	16(%rdx), %r9
	mov	24(%rsi), %r10
	adc	24(%rdx), %r10
	lea	24(%rsi), %rsi
	lea	24(%rdx), %rdx
	lea	24(%rdi), %rdi
	sbb	%ebx, %ebx	

	shrd	$1, %r8, %rbp
	mov	%rbp, -24(%rdi)
	shrd	$1, %r9, %r8
	mov	%r8, -16(%rdi)
.Lcj2:	shrd	$1, %r10, %r9
	mov	%r9, -8(%rdi)
.Lcj1:	mov	%r10, %rbp

.Ldo:
	shr	$2, %rcx			
	je	.Lend			
	.align	16, 0x90
.Ltop:	add	%ebx, %ebx		

	mov	8(%rsi), %r8
	mov	16(%rsi), %r9
	adc	8(%rdx), %r8
	adc	16(%rdx), %r9
	mov	24(%rsi), %r10
	mov	32(%rsi), %r11
	adc	24(%rdx), %r10
	adc	32(%rdx), %r11

	lea	32(%rsi), %rsi
	lea	32(%rdx), %rdx

	sbb	%ebx, %ebx	

	shrd	$1, %r8, %rbp
	mov	%rbp, (%rdi)
	shrd	$1, %r9, %r8
	mov	%r8, 8(%rdi)
	shrd	$1, %r10, %r9
	mov	%r9, 16(%rdi)
	shrd	$1, %r11, %r10
	mov	%r10, 24(%rdi)

	dec	%rcx
	mov	%r11, %rbp
	lea	32(%rdi), %rdi
	jne	.Ltop

.Lend:	shrd	$1, %rbx, %rbp
	mov	%rbp, (%rdi)
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_rsh1add_n,.-__gmpn_rsh1add_n
