








































































					










	.text
	.align	16, 0x90
	.globl	__gmpn_cnd_add_n
	.type	__gmpn_cnd_add_n,@function
	
__gmpn_cnd_add_n:

	

	push	%rbx

	neg	%rdi
	sbb	%rbx, %rbx		

	test	$1, %r8b
	jz	.Lx0
.Lx1:	test	$2, %r8b
	jz	.Lb1

.Lb3:	mov	(%rcx), %rdi
	mov	8(%rcx), %r9
	mov	16(%rcx), %r10
	and	%rbx, %rdi
	and	%rbx, %r9
	and	%rbx, %r10
	add	(%rdx), %rdi
	mov	%rdi, (%rsi)
	adc	8(%rdx), %r9
	mov	%r9, 8(%rsi)
	adc	16(%rdx), %r10
	mov	%r10, 16(%rsi)
	sbb	%eax, %eax	
	lea	24(%rdx), %rdx
	lea	24(%rcx), %rcx
	lea	24(%rsi), %rsi
	sub	$3, %r8
	jnz	.Ltop
	jmp	.Lend

.Lx0:	xor	%eax, %eax
	test	$2, %r8b
	jz	.Ltop

.Lb2:	mov	(%rcx), %rdi
	mov	8(%rcx), %r9
	and	%rbx, %rdi
	and	%rbx, %r9
	add	(%rdx), %rdi
	mov	%rdi, (%rsi)
	adc	8(%rdx), %r9
	mov	%r9, 8(%rsi)
	sbb	%eax, %eax	
	lea	16(%rdx), %rdx
	lea	16(%rcx), %rcx
	lea	16(%rsi), %rsi
	sub	$2, %r8
	jnz	.Ltop
	jmp	.Lend

.Lb1:	mov	(%rcx), %rdi
	and	%rbx, %rdi
	add	(%rdx), %rdi
	mov	%rdi, (%rsi)
	sbb	%eax, %eax	
	lea	8(%rdx), %rdx
	lea	8(%rcx), %rcx
	lea	8(%rsi), %rsi
	dec	%r8
	jz	.Lend

	.align	16, 0x90
.Ltop:	mov	(%rcx), %rdi
	mov	8(%rcx), %r9
	mov	16(%rcx), %r10
	mov	24(%rcx), %r11
	lea	32(%rcx), %rcx
	and	%rbx, %rdi
	and	%rbx, %r9
	and	%rbx, %r10
	and	%rbx, %r11
	add	%eax, %eax	
	adc	(%rdx), %rdi
	mov	%rdi, (%rsi)
	adc	8(%rdx), %r9
	mov	%r9, 8(%rsi)
	adc	16(%rdx), %r10
	mov	%r10, 16(%rsi)
	adc	24(%rdx), %r11
	lea	32(%rdx), %rdx
	mov	%r11, 24(%rsi)
	lea	32(%rsi), %rsi
	sbb	%eax, %eax	
	sub	$4, %r8
	jnz	.Ltop

.Lend:	neg	%eax
	pop	%rbx
	
	ret
	.size	__gmpn_cnd_add_n,.-__gmpn_cnd_add_n
