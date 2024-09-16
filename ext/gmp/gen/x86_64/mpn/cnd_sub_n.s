











































































					




	
	
	







	.text
	.align	16, 0x90
	.globl	__gmpn_cnd_sub_n
	.type	__gmpn_cnd_sub_n,@function
	
__gmpn_cnd_sub_n:

	

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13

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
	mov	(%rdx), %r12
	and	%rbx, %r9
	mov	8(%rdx), %r13
	and	%rbx, %r10
	mov	16(%rdx), %rbp
	sub	%rdi, %r12
	mov	%r12, (%rsi)
	sbb	%r9, %r13
	mov	%r13, 8(%rsi)
	sbb	%r10, %rbp
	mov	%rbp, 16(%rsi)
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
	mov	(%rdx), %r12
	and	%rbx, %rdi
	mov	8(%rdx), %r13
	and	%rbx, %r9
	sub	%rdi, %r12
	mov	%r12, (%rsi)
	sbb	%r9, %r13
	mov	%r13, 8(%rsi)
	sbb	%eax, %eax	
	lea	16(%rdx), %rdx
	lea	16(%rcx), %rcx
	lea	16(%rsi), %rsi
	sub	$2, %r8
	jnz	.Ltop
	jmp	.Lend

.Lb1:	mov	(%rcx), %rdi
	mov	(%rdx), %r12
	and	%rbx, %rdi
	sub	%rdi, %r12
	mov	%r12, (%rsi)
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
	mov	(%rdx), %r12
	and	%rbx, %r9
	mov	8(%rdx), %r13
	and	%rbx, %r10
	mov	16(%rdx), %rbp
	and	%rbx, %r11
	add	%eax, %eax	
	mov	24(%rdx), %rax
	lea	32(%rdx), %rdx
	sbb	%rdi, %r12
	mov	%r12, (%rsi)
	sbb	%r9, %r13
	mov	%r13, 8(%rsi)
	sbb	%r10, %rbp
	mov	%rbp, 16(%rsi)
	sbb	%r11, %rax
	mov	%rax, 24(%rsi)
	lea	32(%rsi), %rsi
	sbb	%eax, %eax	
	sub	$4, %r8
	jnz	.Ltop

.Lend:	neg	%eax
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_cnd_sub_n,.-__gmpn_cnd_sub_n
