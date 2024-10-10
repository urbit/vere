



































































					


	
	
	







	.text
	.align	16, 0x90
	.globl	__gmpn_cnd_sub_n
	.type	__gmpn_cnd_sub_n,@function
	
__gmpn_cnd_sub_n:

	

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14

	neg	%rdi
	sbb	%rdi, %rdi		

	lea	(%rcx,%r8,8), %rcx
	lea	(%rdx,%r8,8), %rdx
	lea	(%rsi,%r8,8), %rsi

	mov	%r8d, %eax
	neg	%r8
	and	$3, %eax
	jz	.Ltop			
	cmp	$2, %eax
	jc	.Lb1
	jz	.Lb2

.Lb3:	mov	(%rcx,%r8,8), %r12
	mov	8(%rcx,%r8,8), %r13
	mov	16(%rcx,%r8,8), %r14
	and	%rdi, %r12
	mov	(%rdx,%r8,8), %r10
	and	%rdi, %r13
	mov	8(%rdx,%r8,8), %rbx
	and	%rdi, %r14
	mov	16(%rdx,%r8,8), %rbp
	sub	%r12, %r10
	mov	%r10, (%rsi,%r8,8)
	sbb	%r13, %rbx
	mov	%rbx, 8(%rsi,%r8,8)
	sbb	%r14, %rbp
	mov	%rbp, 16(%rsi,%r8,8)
	sbb	%eax, %eax	
	add	$3, %r8
	js	.Ltop
	jmp	.Lend

.Lb2:	mov	(%rcx,%r8,8), %r12
	mov	8(%rcx,%r8,8), %r13
	mov	(%rdx,%r8,8), %r10
	and	%rdi, %r12
	mov	8(%rdx,%r8,8), %rbx
	and	%rdi, %r13
	sub	%r12, %r10
	mov	%r10, (%rsi,%r8,8)
	sbb	%r13, %rbx
	mov	%rbx, 8(%rsi,%r8,8)
	sbb	%eax, %eax	
	add	$2, %r8
	js	.Ltop
	jmp	.Lend

.Lb1:	mov	(%rcx,%r8,8), %r12
	mov	(%rdx,%r8,8), %r10
	and	%rdi, %r12
	sub	%r12, %r10
	mov	%r10, (%rsi,%r8,8)
	sbb	%eax, %eax	
	add	$1, %r8
	jns	.Lend

	.align	16, 0x90
.Ltop:	mov	(%rcx,%r8,8), %r12
	mov	8(%rcx,%r8,8), %r13
	mov	16(%rcx,%r8,8), %r14
	mov	24(%rcx,%r8,8), %r11
	and	%rdi, %r12
	mov	(%rdx,%r8,8), %r10
	and	%rdi, %r13
	mov	8(%rdx,%r8,8), %rbx
	and	%rdi, %r14
	mov	16(%rdx,%r8,8), %rbp
	and	%rdi, %r11
	mov	24(%rdx,%r8,8), %r9
	add	%eax, %eax	
	sbb	%r12, %r10
	mov	%r10, (%rsi,%r8,8)
	sbb	%r13, %rbx
	mov	%rbx, 8(%rsi,%r8,8)
	sbb	%r14, %rbp
	mov	%rbp, 16(%rsi,%r8,8)
	sbb	%r11, %r9
	mov	%r9, 24(%rsi,%r8,8)
	sbb	%eax, %eax	
	add	$4, %r8
	js	.Ltop

.Lend:	neg	%eax
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_cnd_sub_n,.-__gmpn_cnd_sub_n
